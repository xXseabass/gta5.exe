import { ExplorerService } from 'backend/explorer/explorer-service';
import { FsService } from 'backend/fs/fs-service';
import { GitService } from 'backend/git/git-service';
import { LogService } from 'backend/logger/log-service';
import { NotificationService } from 'backend/notification/notification-service';
import { SystemResource, SYSTEM_RESOURCES_MAPPING, SYSTEM_RESOURCES_URL } from 'backend/system-resources/system-resources-constants';
import { Task } from 'backend/task/task-reporter-service';
import { injectable, inject } from 'inversify';
import { serverUpdateChannels } from 'shared/api.types';
import { AssetMeta, assetMetaFileExt } from 'shared/asset.types';
import { ProjectManifest } from 'shared/project.types';
import { concurrently } from 'utils/concurrently';
import { omit } from 'utils/omit';
import { endsWith } from 'utils/stringUtils';
import { DEFAULT_PROJECT_SYSTEM_RESOURCES, PROJECT_PATHS_STATE_FILENAME } from '../common/project.constants';
import { ProjectPathsState, ProjectVariableSetter } from '../common/project.types';

export interface ProjectUpgradeRequest {
  task: Task,
  projectPath: string,
  manifestPath: string,
  storagePath: string,
}

@injectable()
export class ProjectUpgrade {
  @inject(FsService)
  protected readonly fsService: FsService;

  @inject(GitService)
  protected readonly gitService: GitService;

  @inject(LogService)
  protected readonly logService: LogService;

  @inject(ExplorerService)
  protected readonly explorerService: ExplorerService;

  @inject(NotificationService)
  protected readonly notificationService: NotificationService;

  private projectFileRestored = false;

  async maybeUpgradeProject(request: ProjectUpgradeRequest) {
    this.maybeTheiaCleanup(request);

    await this.maybeUpgrateAssetMetas(request);
    await this.maybeDeleteStaleCfxServerData(request);

    await this.maybeRestoreProjectFile(request);

    if (!this.projectFileRestored) {
      await this.maybeUpgradeManifestResourcesToAssets(request);
      await this.maybeUpgradeToSystemResources(request);
      await this.maybeUpgradePathsState(request);
      await this.maybeUpgradeUpdatedAt(request);
      await this.maybeUpgradeVariables(request);
    }
  }

  private async maybeRestoreProjectFile(request: ProjectUpgradeRequest) {
    try {
      await this.fsService.readFileJson(request.manifestPath);
    } catch (e) {
      this.projectFileRestored = true;

      const stat = await this.fsService.statSafe(request.manifestPath);
      const defaultDate = new Date().toISOString();

      const manifest: ProjectManifest = {
        createdAt: stat?.birthtime.toISOString() || defaultDate,
        name: this.fsService.basename(request.projectPath),
        serverUpdateChannel: serverUpdateChannels.latest,
        systemResources: DEFAULT_PROJECT_SYSTEM_RESOURCES,
        assets: {},
        variables: {},
      };

      await this.fsService.writeFileJson(request.manifestPath, manifest, true);
    }
  }

  private async maybeUpgrateAssetMetas(request: ProjectUpgradeRequest) {
    const shadowRootPath = this.fsService.joinPath(request.storagePath, 'shadowRoot');

    // If no shadow root path - yay, no need to upgrade metas
    if (!(await this.fsService.statSafe(shadowRootPath))) {
      return;
    }

    request.task.setText('Upgrading project meta files...');

    const paths = await this.explorerService.readDirRecursively(shadowRootPath);

    const entriesToTraverse = new Set(paths[shadowRootPath]);

    for (const entry of entriesToTraverse) {
      if (entry.isDirectory) {
        const entryChildren = paths[entry.path] || [];
        entryChildren.map((childEntry) => entriesToTraverse.add(childEntry));
        continue;
      }

      if (entry.name === 'fxasset.json') {
        const assetRelativePath = this.fsService.relativePath(shadowRootPath, this.fsService.dirname(entry.path));
        const assetMetaPath = this.fsService.joinPath(request.projectPath, assetRelativePath + assetMetaFileExt);

        const oldAssetMeta: Record<string, any> = await this.fsService.readFileJson(entry.path);

        if (oldAssetMeta.flags) {
          const newAssetMeta: AssetMeta = {
            flags: oldAssetMeta.flags,
          };

          await this.fsService.writeFileJson(assetMetaPath, newAssetMeta);
        }
      }
    }

    await this.fsService.rimraf(shadowRootPath);
  }

  private async maybeUpgradeManifestResourcesToAssets(request: ProjectUpgradeRequest) {
    const manifest: any = await this.fsService.readFileJson(request.manifestPath);

    if (!manifest.resources) {
      return;
    }

    manifest.assets = {};

    const resourceNames = new Set(Object.keys(manifest.resources || []));

    if (resourceNames.size > 0) {
      const folderPaths = Object
        .keys(await this.explorerService.readDirRecursively(request.projectPath))
        .filter((folderPath) => folderPath !== request.projectPath && (folderPath.indexOf('.fxdk') === -1))
        .sort((a, b) => a.length - b.length);

      for (const resourceName of resourceNames) {
        const resourcePath = folderPaths.find((folderPath) => endsWith(folderPath, resourceName));

        if (resourcePath) {
          const assetRelativePath = this.fsService.relativePath(request.projectPath, resourcePath);

          manifest.assets[assetRelativePath] = {
            enabled: false,
            ...omit(manifest.resources[resourceName] || {}, 'name'),
          };

          continue;
        }
      }
    }

    delete manifest.resources;

    await this.fsService.writeFileJson(request.manifestPath, manifest, true);
  }

  private async maybeUpgradeToSystemResources(request: ProjectUpgradeRequest) {
    const currentSysresPath = this.fsService.joinPath(request.projectPath, 'system-resources');
    const legacySysresPath = this.fsService.joinPath(request.projectPath, 'cfx-server-data');

    let sysresPath: string;

    if (await this.fsService.statSafe(currentSysresPath)) {
      sysresPath = currentSysresPath;
    } else if (await this.fsService.statSafe(legacySysresPath)) {
      sysresPath = legacySysresPath;
    } else {
      // no system-resources it seems, phew
      return;
    }

    const sysresName = this.fsService.basename(sysresPath);

    if (!(await this.fsService.statSafe(this.fsService.joinPath(sysresPath, '.git')))) {
      // not a git repo, so also bail out
      return;
    }

    const sysresOriginUrl = await this.gitService.getOriginRemoteUrl(sysresPath);
    if (sysresOriginUrl !== SYSTEM_RESOURCES_URL) {
      // not our boy, yay
      return;
    }

    await this.fsService.rimraf(sysresPath + '.fxmeta');

    try {
      const hasUncommitedChanges = (await this.gitService.getUncommitedChanges(sysresPath)).length !== 0;
      const hasDifferentBranches = (await this.gitService.listLocalBranches(sysresPath)).some((branchName) => branchName !== 'master');
      const hasDiverted = await this.gitService.hasBranchDirevertedFromRemote(sysresPath, 'master');

      if (!hasDifferentBranches) {
        await this.fsService.rimraf(this.fsService.joinPath(sysresPath, '.git'));
      }

      // people do be weird modifying read-only shit, right
      if (hasUncommitedChanges || hasDifferentBranches || hasDiverted) {
        const manifest = await this.fsService.readFileJson<ProjectManifest>(request.manifestPath);

        const newSysresPath = this.fsService.joinPath(request.projectPath, 'modified-system-resources');

        await this.fsService.rename(sysresPath, newSysresPath);

        manifest.assets = Object.entries(manifest.assets).reduce((acc, [assetRelativePath, assetConfig]) => {
          if (assetRelativePath.indexOf(sysresName) === 0) {
            const newAssetRelativePath = this.fsService.joinPath(
              'modified-system-resources',
              this.fsService.relativePath(sysresName, assetRelativePath),
            );

            acc[newAssetRelativePath] = assetConfig;
          } else {
            acc[assetRelativePath] = assetConfig;
          }

          return acc;
        }, {});

        (manifest as any).pathsState = Object.keys((manifest as any).pathsState).reduce((acc, dirPath) => {
          if (dirPath.indexOf(sysresPath) !== 0) {
            acc[dirPath] = (manifest as any).pathsState[dirPath];
          }

          return acc;
        }, {});

        await this.fsService.writeFileJson(request.manifestPath, manifest);

        this.notificationService.warning(
          `You have modified "${this.fsService.basename(sysresPath)}" even though it is/was marked read-only` +
          `, this now deprecated folder can't be removed automatically, instead it was renamed to "modified-system-resources".`,
        );
        return;
      }

      await this.fsService.rimraf(sysresPath);
    } catch (e) {
      // oh well, can't do much now
      return;
    }

    const manifest = await this.fsService.readFileJson<ProjectManifest>(request.manifestPath);
    const manifestSystemResources = new Set(manifest.systemResources);

    const exampleLoadscreenRelativePath = this.fsService.joinPath(sysresName, 'resources\\[test]\\example-loadscreen');
    if (manifest.assets[exampleLoadscreenRelativePath]) {
      try {
        const importTo = this.fsService.joinPath(request.projectPath, '[imported]\\example-loadscreen');
        const importFrom = this.fsService.joinPath(sysresPath, '[test]\\example-loadscreen');

        await this.fsService.mkdirp(importTo);
        await this.fsService.copyDirContent(importFrom, importTo);

        manifest.assets['[imported]\\example-loadscreen'] = manifest.assets[exampleLoadscreenRelativePath];
        delete manifest.assets[exampleLoadscreenRelativePath];
      } catch (e) {
        this.notificationService.error(`Error during project upgrade: Failed to import example-loadscreen: ${e.toString()}`);
      }
    }

    const fivemRelativePath = this.fsService.joinPath(sysresName, 'resources\\[test]\\fivem');
    if (manifest.assets[fivemRelativePath]) {
      manifestSystemResources.add(SystemResource.BASIC_GAMEMODE);
      delete manifest.assets[fivemRelativePath];
    }

    // remove all sysres assets
    manifest.assets = Object.entries(manifest.assets).reduce((acc, [assetRelativePath, assetConfig]) => {
      if (assetRelativePath.indexOf(sysresName) === 0) {
        if (assetConfig.enabled) {
          const found: [SystemResource, string] = Object.entries(SYSTEM_RESOURCES_MAPPING)
            .find(([, relPath]) => {
              return this.fsService.joinPath(sysresName, 'resources', relPath) === assetRelativePath;
            }) as any;

          if (found) {
            manifestSystemResources.add(found[0]);
          }
        }
      } else {
        acc[assetRelativePath] = assetConfig;
      }

      return acc;
    }, {});

    // remove all sysres pathsStates
    (manifest as any).pathsState = Object.keys((manifest as any).pathsState).reduce((acc, dirPath) => {
      if (dirPath.indexOf(sysresPath) !== 0) {
        acc[dirPath] = (manifest as any).pathsState[dirPath];
      }

      return acc;
    }, {});

    manifest.systemResources = [...manifestSystemResources];

    await this.fsService.writeFileJson(request.manifestPath, manifest, true);

    this.notificationService.info(
      'Hey, in-project system-resources is now deprected thus it has been deleted. ' +
      'You can now control what system resources your project needs in Project Settings.',
    );
  }

  private async maybeUpgradePathsState(request: ProjectUpgradeRequest) {
    const pathsStatePath = this.fsService.joinPath(request.storagePath, PROJECT_PATHS_STATE_FILENAME);
    if (await this.fsService.statSafe(pathsStatePath)) {
      // already upgraded
      return;
    }

    const manifest = await this.fsService.readFileJson<ProjectManifest>(request.manifestPath);

    const pathsState: ProjectPathsState | void = (manifest as any).pathsState;
    if (!pathsState) {
      // wtf, but ok
      return this.fsService.writeFileJson(pathsStatePath, {});
    }

    delete (manifest as any).pathsState;

    await concurrently(
      this.fsService.writeFileJson(pathsStatePath, pathsState),
      this.fsService.writeFileJson(request.manifestPath, manifest),
    );
  }

  private async maybeUpgradeUpdatedAt(request: ProjectUpgradeRequest) {
    const manifest = await this.fsService.readFileJson(request.manifestPath);

    if ((manifest as any).updatedAt) {
      delete (manifest as any).updatedAt;

      return this.fsService.writeFileJson(request.manifestPath, manifest);
    }
  }

  private async maybeUpgradeVariables(request: ProjectUpgradeRequest) {
    const manifest: any = await this.fsService.readFileJson(request.manifestPath);
    if (!manifest.variables) {
      return;
    }

    const oldValues = Object.values(manifest.variables);
    if (!oldValues.length) {
      return;
    }

    if (typeof oldValues[0] === 'object') {
      return;
    }

    const newVariables: ProjectManifest['variables'] = {};

    for (const [variable, value] of Object.entries(manifest.variables)) {
      if (typeof value !== 'object') {
        newVariables[variable] = {
          value: value as any,
          setter: ProjectVariableSetter.SERVER_ONLY,
        };
      } else if (value !== null && ('setter' in value) && ('value' in value)) {
        newVariables[variable] = value as any;
      }
    }

    manifest.variables = newVariables;

    return this.fsService.writeFileJson(request.manifestPath, manifest);
  }

  private async maybeDeleteStaleCfxServerData(request: ProjectUpgradeRequest) {
    const entryPath = this.fsService.joinPath(request.projectPath, 'cfx-server-data');
    const entryMetaPath = this.fsService.joinPath(request.projectPath, 'cfx-server-data.fxmeta');

    const [hasEntry, hasEntryMeta] = await Promise.all([
      this.fsService.statSafe(entryPath),
      this.fsService.statSafe(entryMetaPath),
    ]);

    if (hasEntryMeta && !hasEntry) {
      try {
        await this.fsService.unlink(entryMetaPath);
      } catch (e) {
        // don't care as doesn't affect anything
      }
    }
  }

  private async maybeTheiaCleanup(request: ProjectUpgradeRequest) {
    const theiaSettingsPath = this.fsService.joinPath(request.storagePath, 'theia-settings.json');

    if (await this.fsService.statSafe(theiaSettingsPath)) {
      try {
        await this.fsService.unlink(theiaSettingsPath);
      } catch (e) {
        // don't really care
      }
    }
  }
}
