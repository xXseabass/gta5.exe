import { inject, injectable } from "inversify";
import { ApiContribution } from "backend/api/api.extensions";
import { AppContribution } from "backend/app/app.extensions";
import { ServerStates, ServerUpdateStates } from 'shared/api.types';
import { handlesClientEvent } from 'backend/api/api-decorators';
import { serverApi } from 'shared/api.events';
import { FsService } from 'backend/fs/fs-service';
import { LogService } from 'backend/logger/log-service';
import { GameServerManagerService } from './game-server-manager-service';
import { ApiClient } from 'backend/api/api-client';
import { TaskReporterService } from 'backend/task/task-reporter-service';
import { NotificationService } from 'backend/notification/notification-service';
import { GameService } from 'backend/game/game-service';
import { Deferred } from 'backend/deferred';
import { isPortAvailable } from 'backend/net-utils';
import { ContainerAccess } from 'backend/container-access';
import { GameServerMode } from './game-server-interface';
import { GameServerRuntime, ServerResourceDescriptor, ServerStartRequest, ServerVariableDescriptor } from "./game-server-runtime";
import { GameServerFxdkMode } from './game-server-fxdk-mode';
import { GameServerLegacyMode } from "./game-server-legacy-mode";
import { SingleEventEmitter } from "utils/singleEventEmitter";
import { IDisposable } from "fxdk/base/disposable";
import { ProjectEvents } from "fxdk/project/node/project-events";

@injectable()
export class GameServerService implements AppContribution, ApiContribution {
  getId() {
    return 'GameServerService';
  }

  @inject(ApiClient)
  protected readonly apiClient: ApiClient;

  @inject(FsService)
  protected readonly fsService: FsService;

  @inject(LogService)
  protected readonly logService: LogService;

  @inject(TaskReporterService)
  protected readonly taskReporterService: TaskReporterService;

  @inject(NotificationService)
  protected readonly notificationService: NotificationService;

  @inject(GameService)
  protected readonly gameService: GameService;

  @inject(GameServerManagerService)
  protected readonly gameServerManagerService: GameServerManagerService;

  @inject(ContainerAccess)
  protected readonly containerAccess: ContainerAccess;

  @inject(GameServerRuntime)
  protected readonly gameServerRuntime: GameServerRuntime;

  protected state = ServerStates.down;

  protected server: GameServerMode | null = null;

  protected serverLock = new Deferred<void>();
  protected serverLocked = false;

  private readonly serverStopEvent = new SingleEventEmitter<Error | void>();
  onServerStop(cb: (error: Error | void) => void): IDisposable {
    return this.serverStopEvent.addListener(cb);
  }

  private readonly serverStateChangeEvent = new SingleEventEmitter<ServerStates>();
  onServerStateChange(cb: (serverStart: ServerStates) => void): IDisposable {
    return this.serverStateChangeEvent.addListener(cb);
  }

  private disposeServer() {
    if (this.server) {
      this.server.dispose?.();
      this.server = null;
    }
  }

  boot() {
    ProjectEvents.BeforeUnload.addListener(() => this.stop());

    this.apiClient.onClientConnected.addListener(() => this.ackState());

    this.serverLock.resolve();
    process.on('exit', () => {
      if (this.server) {
        this.server.stop(this.taskReporterService.create('Stopping server'));
      }
    });
  }

  getState(): ServerStates {
    return this.state;
  }

  isUp() {
    return this.state === ServerStates.up;
  }

  ackState() {
    this.apiClient.emit(serverApi.state, this.state);
  }

  lock() {
    if (this.serverLocked) {
      return;
    }

    this.serverLocked = true;
    this.serverLock = new Deferred();
  }

  unlock() {
    if (!this.serverLocked) {
      return;
    }

    this.serverLocked = false;
    this.serverLock.resolve();
  }

  async start(request: ServerStartRequest) {
    if (this.server) {
      return this.notificationService.warning('Failed to start server as it is already running');
    }

    const { fxserverCwd, updateChannel } = request;

    await this.serverLock.promise;

    this.lock();

    await this.gameServerManagerService.getUpdateChannelPromise(request.updateChannel, ServerUpdateStates.ready);

    // Check if port is available
    if (!await isPortAvailable(30120)) {
      this.notificationService.error(`Port 30120 is already taken, make sure nothing is using it`);
      return this.unlock();
    }

    this.toState(ServerStates.booting);
    this.logService.log('Starting server', request);

    const startTask = this.taskReporterService.create('Starting server');

    const blankPath = this.fsService.joinPath(fxserverCwd, 'blank.cfg');
    if (!await this.fsService.statSafe(blankPath)) {
      await this.fsService.writeFile(blankPath, '');
    }

    const modeImplService: { new(): GameServerMode } = (await this.gameServerManagerService.getServerSupportsFxdkMode(updateChannel))
      ? GameServerFxdkMode
      : GameServerLegacyMode;

    try {
      this.server = this.containerAccess.resolve(modeImplService);

      this.server.onStop((error) => {
        this.disposeServer();

        if (error) {
          this.notificationService.error(`Server error: ${error.toString()}`);
        }

        this.toState(ServerStates.down);

        this.serverStopEvent.emit(error);
      });

      await this.server.start(request, startTask);

      this.toState(ServerStates.up);

    } catch (e) {
      this.disposeServer();

      this.toState(ServerStates.down);
      this.notificationService.error(`Failed to start server: ${e.toString()}`);

    } finally {
      startTask.done();
      this.unlock();
    }
  }

  async stop() {
    await this.serverLock.promise;

    if (this.server) {
      this.lock();

      const stopTask = this.taskReporterService.create('Stopping server');

      this.gameService.beginUnloading();

      try {
        await this.server.stop(stopTask);
      } catch (e) {
        this.notificationService.error(`Failed to stop server: ${e.toString()}`);
      } finally {
        stopTask.done();
        this.unlock();
      }
    }
  }

  toState(newState: ServerStates) {
    if (this.state === newState) {
      return;
    }

    this.state = newState;
    this.serverStateChangeEvent.emit(this.state);
    this.ackState();
  }

  @handlesClientEvent(serverApi.startResource)
  startResource(resourceName: string) {
    this.gameServerRuntime.startResource(resourceName);
  }

  @handlesClientEvent(serverApi.stopResource)
  stopResource(resourceName: string) {
    this.gameServerRuntime.stopResource(resourceName);
  }

  @handlesClientEvent(serverApi.restartResource)
  restartResource(resourceName: string) {
    this.gameServerRuntime.restartResource(resourceName);
  }

  reloadResource(resourceName: string) {
    this.gameServerRuntime.reloadResource(resourceName);
  }

  setResources(resources: ServerResourceDescriptor[]) {
    this.gameServerRuntime.setResources(resources);
  }
  getResources(): ServerResourceDescriptor[] {
    return this.gameServerRuntime.getResources();
  }

  setVariables(variables: ServerVariableDescriptor[]) {
    this.gameServerRuntime.setVariables(variables);
  }
  getVariables(): ServerVariableDescriptor[] {
    return this.gameServerRuntime.getVariables();
  }

  @handlesClientEvent(serverApi.sendCommand)
  sendCommand(cmd: string) {
    this.gameServerRuntime.sendCommand(cmd);
  }

  @handlesClientEvent(serverApi.ackResourcesState)
  requestResourcesState() {
    this.gameServerRuntime.requestResourcesState();
  }
}
