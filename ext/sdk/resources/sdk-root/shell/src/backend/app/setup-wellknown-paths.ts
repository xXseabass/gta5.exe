import * as fs from 'fs';
import * as path from 'path';
import * as mkdirp from 'mkdirp';
import { ConfigService } from 'backend/config-service';

export function setupWellKnownPaths(config: ConfigService) {
  const wellKnownPaths = {
    'ts_types_path': path.join(config.citizen, 'scripting/v8/index.d.ts').replace('\\', '/'),
  };

  mkdirp.sync(config.sdkStorage);

  fs.writeFileSync(config.wellKnownPathsPath, JSON.stringify(wellKnownPaths));
}
