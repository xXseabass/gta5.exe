import { Pipe, PipeTransform } from '@angular/core';

@Pipe({
    name: 'allowify'
})
export class AllowifyPipe implements PipeTransform {
    transform(value: any) {
        if (!value) {
            return value;
        }

        return value
            .replace(/WHITELIST/g, 'ALLOWLIST')
            .replace(/WhiteList/g, 'WhiteList')
            .replace(/Whitelist/g, 'Allowlist')
            .replace(/whitelist/gi, 'allowlist');
    }
}
