import { inject, injectable } from "inversify";
import { ApiClient } from "backend/api/api-client";
import { statusesApi } from "shared/api.events";
import { ApiContribution } from "backend/api/api.extensions";
import { IDisposableObject } from "fxdk/base/disposable";
import { AppContribution } from "backend/app/app.extensions";

export interface StatusProxy<T> extends IDisposableObject {
  getValue(): T | void;
  setValue(value: T | void): void;
  deleteValue(): void;

  applyValue(cb: (value: T | void) => T | void);
}

@injectable()
export class StatusService implements ApiContribution, AppContribution {
  getId() {
    return 'StatusService';
  }

  private statuses: Record<string, any> = {};

  @inject(ApiClient)
  protected readonly apiClient: ApiClient;

  boot() {
    this.apiClient.onClientConnected.addListener(() => this.ack());
  }

  get<T>(statusName: string): T | void {
    return this.statuses[statusName];
  }

  set<T>(statusName: string, statusContent: T) {
    this.statuses[statusName] = statusContent;

    this.ackClient(statusName, statusContent);
  }

  delete(statusName: string) {
    delete this.statuses[statusName];

    this.ackClient(statusName, null);
  }

  createProxy<T>(statusName: string): StatusProxy<T> {
    return new StatusProxyImpl(this, statusName);
  }

  private ack() {
    this.apiClient.emit(statusesApi.statuses, this.statuses);
  }

  private ackClient(statusName: string, statusContent: any) {
    this.apiClient.emit(statusesApi.update, [statusName, statusContent]);
  }
}

class StatusProxyImpl<T> implements StatusProxy<T> {
  constructor(
    protected readonly statusService: StatusService,
    protected readonly statusName: string,
  ) { }

  setValue(value: T | void) {
    this.statusService.set(this.statusName, value);
  }

  getValue(): T | void {
    return this.statusService.get<T>(this.statusName);
  }

  deleteValue() {
    this.statusService.delete(this.statusName);
  }

  applyValue(cb: (value: T | void) => T | void) {
    this.setValue(cb(this.getValue()));
  }

  dispose() {
    this.deleteValue();
  }
}
