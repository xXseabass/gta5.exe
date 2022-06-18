import fs from 'fs';
import cp from 'child_process';
import { inject, injectable } from "inversify";
import { ApiClient } from "backend/api/api-client";
import { StatusService } from "backend/status/status-service";
import { AppContribution } from "backend/app/app.extensions";
import { Deferred } from "backend/deferred";
import { FsService } from "backend/fs/fs-service";
import { LogService } from "backend/logger/log-service";
import { featuresStatuses } from "shared/api.statuses";
import { Feature, FeaturesMap } from "shared/api.types";
import { concurrently } from 'utils/concurrently';
import { ConfigService } from 'backend/config-service';

interface LoadableFeature {
  key: string,
  feature: Feature,
  enabledByDefault: boolean,
}

const loadableFeatures: LoadableFeature[] = [
  {
    key: 'world-editor',
    feature: Feature.worldEditor,
    enabledByDefault: true,
  }
];

@injectable()
export class FeaturesService implements AppContribution {
  protected defers: Partial<Record<Feature, Deferred<boolean>>> = {};

  @inject(ApiClient)
  protected readonly apiClient: ApiClient;

  @inject(StatusService)
  protected readonly statusService: StatusService;

  @inject(FsService)
  protected readonly fsService: FsService;

  @inject(LogService)
  protected readonly logService: LogService;

  @inject(ConfigService)
  protected readonly configService: ConfigService;

  boot() {
    this.probeFeatures();
  }

  async get(feature: Feature): Promise<boolean> {
    const featureState = this.state[feature];

    if (typeof featureState === 'boolean') {
      return featureState;
    }

    let featureDefer = this.defers[feature];
    if (!featureDefer) {
      featureDefer = this.defers[feature] = new Deferred();
    }

    return featureDefer.promise;
  }

  private get state(): FeaturesMap {
    return this.statusService.get<FeaturesMap>(featuresStatuses.state) || {};
  }

  private resolveFeature(feature: Feature, featureState: boolean) {
    const state = this.state;

    state[feature] = featureState;

    this.statusService.set(featuresStatuses.state, state);

    const featureDefer = this.defers[feature];
    if (featureDefer) {
      featureDefer.resolve(featureState);
    }

    this.logService.log(`Feature "${Feature[feature]}"? ${featureState}.`);
  }

  private async probeFeatures() {
    this.logService.log('Start probing features...');

    await concurrently(
      this.probeWindowsDevMode(),
      this.probeDotnet(),
      this.loadFeatures(),
    );
  }

  protected async probeWindowsDevMode() {
    let windowsDevModeEnabled: boolean;

    const tmpdir = this.fsService.tmpdir();

    const source = this.fsService.joinPath(tmpdir, '__fxdk_devmode_feature_probe_source');
    const target = this.fsService.joinPath(tmpdir, '__fxdk_devmode_feature_probe_target');


    await Promise.all([
      await this.fsService.rimraf(source),
      await this.fsService.rimraf(target),
    ]);

    await this.fsService.mkdirp(source);

    try {
      await fs.promises.symlink(source, target, 'dir');

      windowsDevModeEnabled = true;
    } catch (e) {
      windowsDevModeEnabled = false;
    }

    this.resolveFeature(Feature.windowsDevModeEnabled, windowsDevModeEnabled);

    await Promise.all([
      await this.fsService.rimraf(source),
      await this.fsService.rimraf(target),
    ]);
  }

  protected async probeDotnet() {
    let dotnetAvailable = false;

    try {
      cp.execSync('dotnet --version');

      dotnetAvailable = true;
    } catch (e) {
      // unavailable
    }

    this.resolveFeature(Feature.dotnetAvailable, dotnetAvailable);
  }

  protected async loadFeatures() {
    let enabledFeatures: string[] = [];

    if (await this.fsService.statSafe(this.configService.featuresFilePath)) {
      try {
        enabledFeatures = await this.fsService.readFileJson(this.configService.featuresFilePath);

        if (!Array.isArray(enabledFeatures)) {
          throw new Error('Features file must be a valid JSON file with array in it');
        }
      } catch (e) {
        enabledFeatures = [];

        this.logService.error(new Error('Failed to read features file'), { originalError: e });
      }
    }

    for (const { key, feature, enabledByDefault } of loadableFeatures) {
      const enabled = enabledFeatures.includes(key) || enabledByDefault;

      this.resolveFeature(feature, enabled);
    }
  }
}
