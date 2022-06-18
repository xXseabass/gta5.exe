import {
	Component, Input, NgZone, Inject, PLATFORM_ID, ChangeDetectorRef,
	ChangeDetectionStrategy, ElementRef, ViewChild, OnInit, OnDestroy
} from '@angular/core';
import { Server } from '../../server';
import { PinConfigCached } from '../../pins';

import { isPlatformBrowser } from '@angular/common';

import { ServersService, HistoryServerStatus, HistoryServer } from '../../servers.service';
import { FiltersService } from '../../filters.service';
import { GameService } from 'app/game.service';
import { DomSanitizer } from '@angular/platform-browser';
import { L10nLocale, L10N_LOCALE } from 'angular-l10n';
import { master } from 'app/servers/master';

@Component({
	moduleId: module.id,
	selector: 'servers-list',
	templateUrl: 'servers-list.component.html',
	styleUrls: ['servers-list.component.scss'],
	changeDetection: ChangeDetectionStrategy.OnPush
})
export class ServersListComponent implements OnInit, OnDestroy {
	@Input()
	private pinConfig: PinConfigCached;

	@Input()
	type: string;

	private subscriptions: { [addr: string]: any } = {};

	@ViewChild('list')
	private list: ElementRef;

	HistoryServerStatus = HistoryServerStatus;

	serversLoaded = false;
	sortingComplete = false;

	sortedServers: master.IServer[] = [];
	historyServers: HistoryServer[] = [];

	screenHeight = 1080;

	private updateScreenHeight = () => {
		this.screenHeight = window.screen.availHeight;
	}

	constructor(
		private zone: NgZone,
		@Inject(PLATFORM_ID) private platformId: any,
		public changeDetectorRef: ChangeDetectorRef,
		private serversService: ServersService,
		private filtersService: FiltersService,
		private gameService: GameService,
		private sanitizer: DomSanitizer,
		@Inject(L10N_LOCALE) public locale: L10nLocale,
	) {
		this.updateScreenHeight();

		this.serversService.serversLoadedUpdate.subscribe((loaded) => {
			this.serversLoaded = true;
			this.changeDetectorRef.markForCheck();
		});

		this.filtersService.sortingInProgress.subscribe((inProgress) => {
			this.sortingComplete = !inProgress;
			this.changeDetectorRef.markForCheck();
		});

		const finalFilter = this.finalFilter.bind(this);

		this.filtersService.sortedServersUpdate.subscribe((servers) => {
			this.sortedServers = servers.filter(finalFilter);

			if (this.type === 'pins') {
				this.sortedServers.sort((a, b) => this.serversService.getMaterializedServer(b).currentPlayers -
					this.serversService.getMaterializedServer(a).currentPlayers);
			}

			this.changeDetectorRef.markForCheck();
		});
	}

	get shouldShowHint() {
		if (this.type !== 'history' && this.type !== 'favorites') {
			return false;
		}

		if (!this.serversLoaded || !this.sortingComplete) {
			return false;
		}

		return this.sortedServers.length === 0;
	}

	get hintText() {
		switch (this.type) {
			case 'history': return '#EmptyServers_History';
			case 'favorites': return '#EmptyServers_Favorites';
		}
	}

	finalFilter(server: master.IServer) {
		if (this.type === 'pins') {
			return this.isPinned(server);
		} else if (Object.entries(this.filtersService.tags.localeList).length > 0 ||
			Object.entries(this.filtersService.tags.tagList).length > 0) {
			return !this.isPinned(server);
		}

		return true;
	}

	isBrowser() {
		return isPlatformBrowser(this.platformId);
	}

	isPinned(server: master.IServer) {
		return this.filtersService.pinConfig.pinnedServers.has(server?.EndPoint);
	}

	isPremium(server: Server) {
		return !!server.premium;
	}

	getPremium(server: Server) {
		return server.premium;
	}

	ngOnInit() {
		window.addEventListener('resize', this.updateScreenHeight);

		if (this.type === 'history') {
			this.historyServers = this.gameService
				.getServerHistory()
				.slice(-6)
				.map((historyEntry) => ({
					historyEntry,
					server: null,
					sanitizedIcon: (historyEntry.icon ? this.sanitizer.bypassSecurityTrustUrl(historyEntry.icon) : null),
					status: HistoryServerStatus.Loading,
				}))
				.reverse();

			this.historyServers.forEach(async (historyServer) => {
				const isAddressServer = historyServer.historyEntry.address.includes(':');

				if (!isAddressServer) {
					try {
						historyServer.server = await this.serversService.getServer(historyServer.historyEntry.address);
						historyServer.status = HistoryServerStatus.Online;
					} catch (e) {
						historyServer.status = HistoryServerStatus.Offline;
					}
				} else {
					try {
						historyServer.server = await this.gameService.queryAddress(
							this.serversService.parseAddress(historyServer.historyEntry.address),
						);

						historyServer.status = HistoryServerStatus.Online;
					} catch (e) {
						historyServer.status = HistoryServerStatus.Offline;
					}
				}

				this.changeDetectorRef.markForCheck();
			});
		} else {
			this.filtersService.sortAndFilterServers(false);
		}
	}

	ngOnDestroy() {
		window.removeEventListener('resize', this.updateScreenHeight);
	}

	attemptConnectTo(entry: HistoryServer) {
		if (!entry.server) {
			return;
		}

		this.gameService.connectTo(entry.server, entry.historyEntry.title || entry.historyEntry.address);
	}

	svTrack(_: number, serverRow: master.IServer) {
		return serverRow.EndPoint;
	}

	isFavorite(server: master.IServer) {
		return this.gameService.isMatchingServer('favorites', server);
	}

	toggleFavorite(event, server: master.IServer) {
		if (this.isFavorite(server)) {
			this.gameService.toggleListEntry('favorites', this.serversService.getMaterializedServer(server), false);
		} else {
			this.gameService.toggleListEntry('favorites', this.serversService.getMaterializedServer(server), true);
		}

		event.stopPropagation();
	}

	makeServer(server: Server): master.IServer {
		return {
			EndPoint: server.address,
			Data: server.data
		};
	}
}
