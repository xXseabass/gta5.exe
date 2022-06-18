import {DomSanitizer} from '@angular/platform-browser';
import { EventEmitter } from '@angular/core';

import { master } from './master';

import { Avatar } from './avatar';

export class Server {
    readonly hostname: string;
    readonly sortname: string;
    readonly strippedname: string;
    readonly maxPlayers: number;
    readonly data: any;
    readonly int: master.IServerData;

    connectEndPoints: string[];
    address: string;

	private _live: boolean;

	iconNeedsResolving = true;
	cachedResolvedIcon: HTMLImageElement;

    bitmap: ImageBitmap;
    onChanged = new EventEmitter<void>();

    realIconUri: string;

    get iconUri(): string {
		if (!this.realIconUri) {
			this.setDefaultIcon();
		}

        return this.realIconUri;
    }

    set iconUri(value: string) {
        this.realIconUri = value;

        if (this.sanitizer) {
            this.sanitizedUri = this.sanitizer.bypassSecurityTrustUrl(value);
            this.sanitizedStyleUri = this.sanitizer.bypassSecurityTrustStyle('url(' + value + ')');
        }
	}

	get premium(): string {
		return this.data?.vars?.premium || '';
	}

    sanitizedUri: any;
    sanitizedStyleUri: any;
    currentPlayers: number;
    ping = 9999;
    upvotePower = 0;
	burstPower = 0;
    isDefaultIcon = false;

    public static fromObject(sanitizer: DomSanitizer, address: string, object: master.IServerData): Server {
        return new Server(sanitizer, address, object);
    }

    public static fromNative(sanitizer: DomSanitizer, object: any): Server {
        const mappedData = {
            hostname: object.name || object.hostname,
            clients: object.clients,
            svMaxclients: object.maxclients ?? object.sv_maxclients,
            resources: [],
            mapname: object.mapname,
            gametype: object.gametype
        };

        const server = new Server(sanitizer, object.addr, { ...object.infoBlob || {}, ...object, ...mappedData });

        if (object.infoBlob) {
            if (object.infoBlob.icon) {
                server.iconUri = 'data:image/png;base64,' + object.infoBlob.icon;
            }
        }

        return server;
    }

    public setDefaultIcon() {
        const svg = Avatar.getFor(this.address);

        this.iconUri = `data:image/svg+xml,${encodeURIComponent(svg)}`;
        this.isDefaultIcon = true;
    }

    public updatePing(newValue: number): void {
        this.ping = newValue;

        this.onChanged.emit();
    }

    public getSortable(name: string): any {
        switch (name) {
            case 'name':
                return this.sortname;
            case 'ping':
                return this.ping;
            case 'players':
                return this.currentPlayers;
            case 'upvotePower':
                return this.upvotePower;
            default:
                throw new Error('Unknown sortable');
        }
    }

    private constructor(private sanitizer: DomSanitizer, address: string, object: master.IServerData) {
        // temp compat behavior
        this.address = address;

        if (!object) {
            return;
        }

        this.hostname = object.hostname;
        this.currentPlayers = object.clients | 0;
        this.maxPlayers = object.svMaxclients | 0;

        this.strippedname = (this.hostname || '').replace(/\^[0-9]/g, '').normalize('NFD').replace(/[\u0300-\u036f]/g, '');
        this.sortname = this.strippedname.replace(/[^a-zA-Z0-9]/g, '').replace(/^[0-9]+/g, '').toLowerCase();

        // only weird characters? sort on the bottom
        if (this.sortname.length === 0) {
            this.sortname = 'z';
        }

        if (object.vars && object.vars.ping) {
            this.ping = parseInt(object.vars.ping, 10);
            delete object.vars['ping'];
        }

        this.upvotePower = object.upvotePower || 0;
        this.burstPower = object.burstPower || 0;
        this.data = object;
        this.int = object;
        this.connectEndPoints = object.connectEndPoints;

        if (object.iconVersion) {
            this.iconUri = `https://servers-live.fivem.net/servers/icon/${address}/${object.iconVersion}.png`;
        }
    }
}

export class ServerIcon {
    readonly addr: string;
    readonly icon: string;
    readonly iconVersion: number;

    public constructor(addr: string, icon: string, iconVersion: number) {
        this.addr = addr;
        this.icon = icon;
        this.iconVersion = iconVersion;
    }
}

export class ServerHistoryEntry {
	address: string;
	title: string;
	hostname: string;
	time: Date;
	icon: string;
	token: string;
	rawIcon: string;
	vars: { [key: string]: string };
}
