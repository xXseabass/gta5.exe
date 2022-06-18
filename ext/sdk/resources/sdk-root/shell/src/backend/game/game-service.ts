import { ApiClient } from "backend/api/api-client";
import { ApiContribution } from "backend/api/api.extensions";
import { handlesClientCallbackEvent, handlesClientEvent } from "backend/api/api-decorators";
import { AppContribution } from "backend/app/app.extensions";
import { ConfigService } from "backend/config-service";
import { Deferred } from "backend/deferred";
import { IDisposable } from "fxdk/base/disposable";
import { FsService } from "backend/fs/fs-service";
import { LogService } from "backend/logger/log-service";
import { NotificationService } from "backend/notification/notification-service";
import { emitSystemEvent, onSystemEvent } from "backend/system-events";
import { WorldEditorArchetypesService } from "backend/world-editor/world-editor-archetypes-service";
import { inject, injectable } from "inversify";
import { gameApi } from "shared/api.events";
import { NetLibraryConnectionState, SDKGameProcessState } from "shared/native.enums";
import { SingleEventEmitter } from "utils/singleEventEmitter";
import { GameStates } from "./game-constants";

@injectable()
export class GameService implements ApiContribution, AppContribution {
  getId(): string {
    return 'GameService';
  }

  @inject(FsService)
  protected readonly fsService: FsService;

  @inject(ApiClient)
  protected readonly apiClient: ApiClient;

  @inject(LogService)
  protected readonly logService: LogService;

  @inject(ConfigService)
  protected readonly configService: ConfigService;

  @inject(NotificationService)
  protected readonly notificationService: NotificationService;

  @inject(WorldEditorArchetypesService)
  protected readonly archetypesService: WorldEditorArchetypesService;

  private gameState = GameStates.NOT_RUNNING;

  private gameBuildNumber = 0;

  private gameLaunched = false;

  private gameUnloaded = true;

  private gameProcessState = SDKGameProcessState.GP_STOPPED;

  private connectionState = NetLibraryConnectionState.CS_IDLE;

  private restartPending = false;

  private messageHandlers: Record<string, Set<Function>> = {};

  private archetypesCollectionReady = false;
  private rACDeferred: Deferred<boolean> | null = null;
  private rACTimeout: NodeJS.Timeout | null = null;

  private readonly gameStateChangeEvent = new SingleEventEmitter<GameStates>();
  onGameStateChange(cb: (gameState: GameStates) => void): IDisposable {
    return this.gameStateChangeEvent.addListener(cb);
  }

  getBuildNumber(): number {
    return this.gameBuildNumber;
  }

  getGameState(): GameStates {
    return this.gameState;
  }

  async boot() {
    this.apiClient.onClientConnected.addListener(() => this.ack());

    this.archetypesCollectionReady = Boolean(await this.archetypesService.isValid());

    const gameBuildNumberDeferred = new Deferred<number>();

    gameBuildNumberDeferred.promise.finally(onSystemEvent('sdk:setBuildNumber', (gameBuildNumber: number) => {
      this.gameBuildNumber = gameBuildNumber;

      gameBuildNumberDeferred.resolve();
    }));

    emitSystemEvent('sdk:getBuildNumber');

    on('sdk:gameLaunched', () => {
      this.gameLaunched = true;
      this.gameUnloaded = true;
      this.toGameState(GameStates.READY);

      this.apiClient.emit(gameApi.gameLaunched, true);
    });

    on('sdk:connectionStateChanged', (current: NetLibraryConnectionState, previous: NetLibraryConnectionState) => {
      this.connectionState = current;

      if (this.gameLaunched) {
        if (current === NetLibraryConnectionState.CS_IDLE) {
          this.toGameState(GameStates.UNLOADING);
        } else if (this.gameUnloaded) {
          this.gameUnloaded = false;
          this.toGameState(GameStates.LOADING);
        }
      }

      this.apiClient.emit(gameApi.connectionStateChanged, { current, previous });
    });

    on('sdk:gameProcessStateChanged', (current: SDKGameProcessState, previous: SDKGameProcessState) => {
      this.gameProcessState = current;

      if (this.gameLaunched && (current === SDKGameProcessState.GP_STOPPED || current === SDKGameProcessState.GP_STOPPING)) {
        const previousConnectionState = this.connectionState;

        this.gameLaunched = false;
        this.connectionState = NetLibraryConnectionState.CS_IDLE;
        this.toGameState(GameStates.NOT_RUNNING);

        this.apiClient.emit(gameApi.connectionStateChanged, { current: this.connectionState, previous: previousConnectionState });
        this.apiClient.emit(gameApi.gameLaunched, this.gameLaunched);
      }

      if (current === SDKGameProcessState.GP_STOPPED) {
        this.startGame();

        if (this.restartPending) {
          this.restartPending = false;
        } else {
          this.notificationService.error('It looks like game has crashed, restarting it now', 5000);
        }
      }

      this.apiClient.emit(gameApi.gameProcessStateChanged, { current, previous });
    });

    on('sdk:gameUnloaded', () => {
      this.gameUnloaded = true;

      if (this.gameLaunched) {
        this.toGameState(GameStates.READY);
      }
    });

    on('sdk:backendMessage', (payload: unknown) => {
      if (typeof payload === 'string' && payload) {
        try {
          const { type, data } = JSON.parse(payload);

          const handlers = this.messageHandlers[type];
          if (handlers) {
            for (const handler of handlers) {
              handler(data);
            }
          }
        } catch (e) {
          // don't care
        }
      }

      return;
    });

    on('sdk:refreshArchetypesCollectionDone', () => {
      if (this.rACDeferred) {
        clearTimeout(this.rACTimeout!);
        this.rACTimeout = null;

        this.rACDeferred.resolve(true);
        this.rACDeferred = null;

        this.archetypesService.refresh();
      }
    });

    this.onBackendMessage('connected', () => {
      if (this.gameLaunched) {
        this.toGameState(GameStates.CONNECTED);
      }
    });

    this.startGame();

    await gameBuildNumberDeferred.promise;
  }

  beginUnloading() {
    this.toGameState(GameStates.UNLOADING);
  }

  ack() {
    this.apiClient.emit(gameApi.ack, {
      gameState: this.gameState,
      gameLaunched: this.gameLaunched,
      gameProcessState: this.gameProcessState,
      connectionState: this.connectionState,
      archetypesCollectionReady: this.archetypesCollectionReady,
    });
  }

  @handlesClientEvent(gameApi.start)
  startGame() {
    emit('sdk:startGame');
  }

  @handlesClientEvent(gameApi.stop)
  stopGame() {
    this.gameLaunched = false;
    this.gameUnloaded = true;

    this.toGameState(GameStates.NOT_RUNNING);

    emit('sdk:stopGame');
  }

  @handlesClientEvent(gameApi.restart)
  restartGame() {
    this.restartPending = true;

    this.stopGame();
  }

  onBackendMessage(type: string, handler: Function): IDisposable {
    if (!this.messageHandlers[type]) {
      this.messageHandlers[type] = new Set();
    }

    this.messageHandlers[type].add(handler);

    return () => this.messageHandlers[type].delete(handler);
  }

  emitEvent(eventName: string, payload?: any) {
    emit('sdk:sendGameClientEvent', eventName, JSON.stringify(payload) || '');
  }

  @handlesClientCallbackEvent(gameApi.refreshArchetypesCollection)
  async refreshArchetypesCollection() {
    if (this.rACDeferred) {
      return this.rACDeferred.promise;
    }

    this.rACDeferred = new Deferred();
    this.rACTimeout = setTimeout(() => {
      this.rACDeferred?.reject(new Error('Timedout'));
      this.rACDeferred = null;
      this.rACTimeout = null;
    }, 5 * 60000);// 5 minutes timeout

    emit('sdk:refreshArchetypesCollection');

    return this.rACDeferred.promise;
  }

  private toGameState(newState: GameStates) {
    if (newState === this.gameState) {
      return;
    }

    this.gameState = newState;
    this.gameStateChangeEvent.emit(this.gameState);
    this.apiClient.emit(gameApi.gameState, this.gameState);
  }
}
