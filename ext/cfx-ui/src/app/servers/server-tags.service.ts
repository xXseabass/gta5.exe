import { Injectable, EventEmitter } from '@angular/core';
import { ServersService } from './servers.service';

import { Server } from './server';
import cldrLocales from 'cldr-localenames-modern/main/en/localeDisplayNames.json';
import cldrLanguages from 'cldr-localenames-modern/main/en/languages.json';
import cldrTerritories from 'cldr-localenames-modern/main/en/territories.json';
import cldrSubTags from 'cldr-core/supplemental/likelySubtags.json';
import * as cldrjs from 'cldrjs';
import { getCanonicalLocale } from './components/utils';

export class ServerTag {
	public name: string;
	public count: number;
}

export class ServerLocale {
	public name: string;
	public displayName: string;
	public count: number;
	public countryName: string;
}

cldrjs.load(cldrLocales, cldrLanguages, cldrTerritories, cldrSubTags);

function fromEntries<TValue>(iterable: [string, TValue][]): { [key: string]: TValue } {
	return [...iterable].reduce<{ [key: string]: TValue }>((obj, [key, val]) => {
		(obj as any)[key] = val;
		return obj;
	}, {} as any);
}

@Injectable()
export class ServerTagsService {
	tags: ServerTag[] = [];
	coreTags: { [key: string]: ServerTag } = {};
	locales: ServerLocale[] = [];

	onUpdate = new EventEmitter<void>();

	private tagsIndex: { [key: string]: number } = {};
	private localesIndex: { [key: string]: number } = {};

	constructor(private serversService: ServersService) {
		this.serversService.tagUpdate.subscribe(([tags, locales]) => {
			this.tagsIndex = tags;
			this.localesIndex = locales;

			this.updateTagList();
			this.updateLocaleList();

			this.onUpdate.emit();
		});
	}

	private updateTagList() {
		const tags = Object.entries(this.tagsIndex).sort((a, b) => b[1] - a[1]);
		this.coreTags = fromEntries(tags.map(([name, count]) => ([name, { name, count }])));

		tags.length = Math.min(50, tags.length);

		this.tags = tags.map(([name, count]) => ({ name, count }));
	}

	private updateLocaleList() {
		const locales = Object.entries(this.localesIndex).sort((a, b) => b[1] - a[1]);

		this.locales = locales.map(([name, count]) => ({
			name,
			count,
			displayName: this.getLocaleDisplayName(name),
			countryName: name.split('-').reverse()[0],
		}));
	}

	private cachedLocaleDisplayNames: { [key: string]: string } = {};

	public getLocaleDisplayName(name: string): string {
		let cached = this.cachedLocaleDisplayNames[name];

		if (!this.cachedLocaleDisplayNames[name]) {
			const c = new cldrjs('en');

			const parts = name.split('-');
			const l = parts[0];
			const t = parts[parts.length - 1];

			const lang = c.main('localeDisplayNames/languages/' + l);
			const territory = c.main('localeDisplayNames/territories/' + t);

			cached = this.cachedLocaleDisplayNames[name] = c.main('localeDisplayNames/localeDisplayPattern/localePattern')
				.replace('{0}', lang)
				.replace('{1}', territory);
		}

		return cached;
	}
}
