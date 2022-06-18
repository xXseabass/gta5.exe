import { Component, OnInit, ElementRef, ViewChild, Output, EventEmitter, Inject } from '@angular/core';
import { GameService } from '../game.service';
import * as AdaptiveCards from 'adaptivecards';
import { L10N_LOCALE, L10nLocale } from 'angular-l10n';
import { ActionAlignment } from 'adaptivecards';
import { DiscourseService } from 'app/discourse.service';
import { Avatar } from './avatar';
import { DomSanitizer, SafeHtml } from '@angular/platform-browser';
import { Server } from './server';
import * as markdownIt from 'markdown-it';

// Matches `<ip>:<port>` and `<ip> port <port>`
const ipRegex = Array(4).fill('(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)').join('\\.');
const serverAddressRegex = new RegExp(String.raw`\b${ipRegex}(\s+port\s+|:)\d+\b`, 'g');

const md = markdownIt({
	breaks: true
});

interface ExtraAction {
	title: string;
	action: () => void;
}

@Component({
	moduleId: module.id,
	selector: 'connecting-popup',
	templateUrl: 'connecting-popup.component.html',
	styleUrls: ['connecting-popup.component.scss']
})
export class ConnectingPopupComponent implements OnInit {
	showOverlay = false;
	overlayClosable = true;
	overlayTitle: string;
	overlayMessage: string;
	overlayMessageData: { [key: string]: any } = {};
	overlayBg = '';
	closeLabel = "#Servers_CloseOverlay";
	retryLabel = "#Servers_Retry";
	submitting = false;
	closeKeys = ["Escape"];
    serverLogo: any = '';
	serverData: any = null;
    extraData: any = {};
    lastServer: Server = null;
    statusLevel = 0;

	@Output()
	retry = new EventEmitter();

	@ViewChild('card') cardElement: ElementRef;
	overlayHTML: SafeHtml;
	extraActions: ExtraAction[] = [];

	constructor(
		private gameService: GameService,
        private discourseService: DiscourseService,
        private sanitizer: DomSanitizer,
		@Inject(L10N_LOCALE) public locale: L10nLocale
	) {
        this.serverLogo = this.sanitizer.bypassSecurityTrustStyle(`url('${this.defaultLogo}')`);
	}

	get inSwitchCL() {
		return this.gameService.inSwitchCL;
	}

    get mergedData() {
        return {
            ...this.serverData,
            ...this.extraData
        };
    }

    get userLogo() {
        return this.sanitizer.bypassSecurityTrustStyle(
            `url('${this.discourseService.currentUser?.getAvatarUrl(256) ?? this.defaultLogo}')`);
    }

    get defaultLogo() {
        const svg = Avatar.getFor('meow');

        return `data:image/svg+xml,${encodeURIComponent(svg)}`;
    }

    get youName() {
        return this.discourseService.currentUser?.username ?? this.gameService.nickname;
    }

    get gameName() {
        return this.gameService.gameName;
    }

    get gameBrand() {
        let gameBrand = 'CitizenFX';

        if (this.gameName === 'rdr3') {
            gameBrand = 'RedM';
        } else if (this.gameName === 'gta5') {
            gameBrand = 'FiveM';
        }

        return gameBrand;
    }

    get serverName() {
        if (this.gameService.streamerMode) {
            return '...';
        }

        return (this.serverData?.vars?.sv_projectName ?? this.lastServer?.address ?? 'a server').replace(/\^[0-9]/g, '');
    }

    isCfx(fault: string) {
        return fault.startsWith('cfx');
    }

	ngOnInit() {
		// #TODO: unify the dupe code in these event handlers
		this.gameService.connecting.subscribe(a => {
			this.overlayTitle = '#Servers_Connecting';
			this.overlayMessage = this.gameService.streamerMode
				? '#Servers_ConnectingToServer'
				: '#Servers_ConnectingTo';
			this.overlayMessageData = {
				serverName: a?.address || 'a server',
			};
			this.showOverlay = true;
			this.gameService.showConnectingOverlay = true;
			this.overlayClosable = false;
			this.extraActions = [];

            if (a) {
                this.lastServer = a;
            }

			this.overlayBg = a?.data?.vars?.banner_connecting || '';
			this.serverData = a?.data;
            this.serverLogo = this.sanitizer.bypassSecurityTrustStyle(`url('${a?.iconUri ?? this.defaultLogo}')`);
            this.extraData = {};

			this.clearElements();
		});

		this.gameService.connectFailed.subscribe(([server, message, extra]) => {
			if (message.startsWith('[md]')) {
				this.loadFormattedMessage(message);
			} else {
				this.overlayTitle = '#Servers_ConnectFailed';
				this.overlayMessage = '#Servers_Message';
				this.overlayMessageData = { message };
				this.extraActions = [];
			}

			this.showOverlay = true;
			this.gameService.showConnectingOverlay = true;
			this.overlayClosable = true;
            this.extraData = extra;
			this.closeLabel = "#Servers_CloseOverlay";

            this.updateStatus();

			if (this.gameService.streamerMode && message) {
				this.overlayMessageData.message = message.replace(serverAddressRegex, '&lt;HIDDEN&gt;');
			}

			this.clearElements();
		});

		this.gameService.connectStatus.subscribe(a => {
			if (a.message.startsWith('[md]')) {
				this.loadFormattedMessage(a.message);
			} else {
				this.overlayTitle = '#Servers_Connecting';
				this.overlayMessage = '#Servers_Message';
				this.overlayMessageData = {
					message: a.message,
					serverName: this.gameService.minmodeBlob.productName,
				};

				this.extraActions = [];
			}

			this.showOverlay = true;
			this.gameService.showConnectingOverlay = true;
			this.overlayClosable = a.cancelable;

			if (this.overlayClosable) {
				this.closeLabel = "#Servers_CancelOverlay";
			}

			if (this.gameService.streamerMode && a.message) {
				this.overlayMessageData.message = a.message.replace(serverAddressRegex, '&lt;HIDDEN&gt;');
			}

			this.clearElements();
		});

		this.gameService.connectCard.subscribe(a => {
			const getSize = (size: number) => {
				return (size / 720) * Math.min(window.innerHeight, window.innerWidth);
			}

			this.showOverlay = true;
			this.gameService.showConnectingOverlay = true;

			AdaptiveCards.AdaptiveCard.onProcessMarkdown = (text, result) => {
				result.outputHtml = md.render(text);
				result.didProcess = true;
			};

			const adaptiveCard = new AdaptiveCards.AdaptiveCard();
			adaptiveCard.hostConfig = new AdaptiveCards.HostConfig({
				spacing: {
				},
				supportsInteractivity: true,
				actions: {
					actionsOrientation: 'horizontal',
					actionAlignment: ActionAlignment.Stretch,
				},
				fontTypes: {
					default: {
						fontFamily: 'Rubik',
						fontSizes: {
							small: getSize(8),
							default: getSize(10),
							medium: getSize(13),
							large: getSize(17),
							extraLarge: getSize(22),
						}
					},
					monospace: {
						fontSizes: {
							small: getSize(8),
							default: getSize(10),
							medium: getSize(13),
							large: getSize(17),
							extraLarge: getSize(22),
						}
					}
				},
				imageSizes: {
					small: 40,
					medium: 80,
					large: 120
				},
				imageSet: {
					imageSize: 'medium',
					maxImageHeight: 100
				},
				containerStyles: {
					default: {
						foregroundColors: {
							default: {
								default: '#FFFFFF',
								subtle: '#88FFFFFF',
								highlightColors: {
									default: '#22000000',
									subtle: '#11000000'
								}
							},
							dark: {
								default: '#000000',
								subtle: '#66000000',
								highlightColors: {
									default: '#22000000',
									subtle: '#11000000'
								}
							},
							light: {
								default: '#FFFFFF',
								subtle: '#33000000',
								highlightColors: {
									default: '#22000000',
									subtle: '#11000000'
								}
							},
							accent: {
								default: '#2E89FC',
								subtle: '#882E89FC',
								highlightColors: {
									default: '#22000000',
									subtle: '#11000000'
								}
							},
							good: {
								default: '#00FF00',
								subtle: '#DD00FF00',
								highlightColors: {
									default: '#22000000',
									subtle: '#11000000'
								}
							},
							warning: {
								default: '#FFD800',
								subtle: '#DDFFD800',
								highlightColors: {
									default: '#22000000',
									subtle: '#11000000'
								}
							},
							attention: {
								default: '#FF0000',
								subtle: '#DDFF0000',
								highlightColors: {
									default: '#22000000',
									subtle: '#11000000'
								}
							}
						},
						backgroundColor: '#00000000'
					},
					emphasis: {
						foregroundColors: {
							default: {
								default: '#FFFFFF',
								subtle: '#88FFFFFF',
								highlightColors: {
									default: '#22000000',
									subtle: '#11000000'
								}
							},
							dark: {
								default: '#000000',
								subtle: '#66000000',
								highlightColors: {
									default: '#22000000',
									subtle: '#11000000'
								}
							},
							light: {
								default: '#FFFFFF',
								subtle: '#33000000',
								highlightColors: {
									default: '#22000000',
									subtle: '#11000000'
								}
							},
							accent: {
								default: '#2E89FC',
								subtle: '#882E89FC',
								highlightColors: {
									default: '#22000000',
									subtle: '#11000000'
								}
							},
							good: {
								default: '#00FF00',
								subtle: '#DD00FF00',
								highlightColors: {
									default: '#22000000',
									subtle: '#11000000'
								}
							},
							warning: {
								default: '#FF0000',
								subtle: '#DDFF0000',
								highlightColors: {
									default: '#22000000',
									subtle: '#11000000'
								}
							},
							attention: {
								default: '#FFD800',
								subtle: '#DDFFD800',
								highlightColors: {
									default: '#22000000',
									subtle: '#11000000'
								}
							}
						},
						backgroundColor: '#00000000'
					}
				}
			});

			adaptiveCard.onExecuteAction = (action) => {
				if (action instanceof AdaptiveCards.SubmitAction) {
					if (!this.submitting) {
						let data = action.data as any;

						if (action.id) {
							data = { ...action.data, submitId: action.id };
						}

						// hack
						if (data.action && data.action === 'cancel') {
							setTimeout(() => {
								this.overlayClosable = true;
								this.closeOverlay();
							}, 150);
						}

						this.gameService.submitCardResponse(data);
					}

					this.submitting = true;
				} else if (action instanceof AdaptiveCards.OpenUrlAction) {
					this.gameService.openUrl(action.url);
				}
			};

			adaptiveCard.parse(JSON.parse(a.card));

			const renderedCard = adaptiveCard.render();
			const element = this.cardElement.nativeElement as HTMLElement;

			element.childNodes.forEach(c => element.removeChild(c));
			element.appendChild(renderedCard);

			this.submitting = false;
			this.overlayMessage = '';
		});

		this.gameService.errorMessage.subscribe(message => {
			if (message.startsWith('[md]')) {
				this.loadFormattedMessage(message);
			} else {
				this.overlayTitle = '#Servers_Error';
				this.overlayMessage = '#Servers_Message';
				this.overlayMessageData = { message };

				this.extraActions = [];
			}

			this.showOverlay = true;
			this.gameService.showConnectingOverlay = true;
			this.overlayClosable = true;
			this.closeLabel = "#Servers_CloseOverlay";

			this.clearElements();
		});

		this.gameService.infoMessage.subscribe(message => {
			if (message.startsWith('[md]')) {
				this.loadFormattedMessage(message);
			} else {
				this.overlayTitle = '#Servers_Info';
				this.overlayMessage = '#Servers_Message';
				this.overlayMessageData = { message };

				this.extraActions = [];
			}

			this.showOverlay = true;
			this.gameService.showConnectingOverlay = true;
			this.overlayClosable = true;
			this.closeLabel = "#Servers_CloseOverlay";

			this.clearElements();
		});
	}

	loadFormattedMessage(message: string) {
		message = message.replace(/^\[md\]/, '');
		message = md.render(message);

		const extraActions: ExtraAction[] = [];

		const parser = new DOMParser();
		const doc = parser.parseFromString(`<div>${message}</div>`, 'text/html');
		for (const el of Array.from(doc.querySelectorAll('a[href^="cfx.re://"]'))) {
			const getAction = (action: string) => {
				if (action === 'cfx.re://reconnect') {
					return () => (window as any).invokeNative('reconnect', '');
				}

				return () => {};
			}

			extraActions.push({
				title: el.innerHTML,
				action: getAction((el as HTMLAnchorElement).href)
			});

			el.parentNode.removeChild(el);
		}

		const heading = doc.querySelector('h1');
		if (heading) {
			this.overlayTitle = heading.innerText;
			heading.parentNode.removeChild(heading);
		}

		const serializer = new XMLSerializer();
		const newText = serializer.serializeToString(doc);

		this.overlayMessage = '[html]';
		this.overlayHTML = this.sanitizer.bypassSecurityTrustHtml(newText);
		this.extraActions = extraActions;
	}

	updateStatus() {
		window.fetch('https://status.cfx.re/api/v2/status.json')
			.then(async (res) => {
				const status = (await res.json());
				switch (status['status']['description']) {
					case 'All Systems Operational':
						this.statusLevel = 1;
						break;
					case 'Partial System Outage':
					case 'Minor Service Outage':
						this.statusLevel = 2;
						break;
					case 'Major Service Outage':
						this.statusLevel = 3;
						break;
					default:
						this.statusLevel = 0;
						break;
				}
			})
			.catch(a => {});
	}

	clearElements() {
		const element = this.cardElement.nativeElement as HTMLElement;

		element.childNodes.forEach(c => element.removeChild(c));
	}

	clickContent(event: MouseEvent) {
		const srcElement = event.target as HTMLElement;

		if (srcElement.localName === 'a') {
			this.gameService.openUrl(srcElement.getAttribute('href'));

			event.preventDefault();
		}
	}

	closeOverlay() {
		if (this.overlayClosable) {
			this.showOverlay = false;
			this.gameService.showConnectingOverlay = false;
			this.gameService.inSwitchCL = false;

			this.gameService.cancelNativeConnect();
		}
	}

	doRetry() {
		this.retry.emit();
	}

	onKeyPress(event: KeyboardEvent) {
		if (this.closeKeys.includes(event.code)) {
			this.closeOverlay()
		}
	}

	replaceFn(mitch: any): boolean {
		if (mitch.getType() === 'url') {
			const url = mitch.getUrl();

			if (url.toLowerCase().indexOf('cfx.re') !== -1) {
				return false;
			}
		}

		return true;
	}
}
