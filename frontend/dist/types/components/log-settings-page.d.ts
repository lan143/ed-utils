import { LitElement } from 'lit';
import type { TemplateResult } from 'lit';
import type { LogSettings } from '../api/types';
/** Detail of the `edu-log-saved` event: the device's current settings. */
export type LogSettingsSavedDetail = LogSettings;
type NoticeKind = 'info' | 'error';
interface Notice {
    kind: NoticeKind;
    text: string;
}
/**
 * Network-logger settings panel for ed-utils devices.
 *
 * A small form over `GET`/`POST /api/log/settings`. The device echoes its
 * current state after a successful save, and that echo is the source of truth:
 * the form is refreshed from it after every accepted POST.
 */
export declare class EduLogSettingsPage extends LitElement {
    static properties: {
        baseUrl: {
            type: StringConstructor;
            attribute: string;
        };
        settingsPath: {
            type: StringConstructor;
            attribute: string;
        };
        deviceName: {
            type: StringConstructor;
            attribute: string;
        };
        maxHostLength: {
            type: NumberConstructor;
            attribute: string;
        };
        formHost: {
            state: boolean;
        };
        formUri: {
            state: boolean;
        };
        formPort: {
            state: boolean;
        };
        formEnabled: {
            state: boolean;
        };
        loading: {
            state: boolean;
        };
        saving: {
            state: boolean;
        };
        notice: {
            state: boolean;
        };
        loaded: {
            state: boolean;
        };
    };
    static styles: import("lit").CSSResult;
    /** Prefix for every API request. Default '' = same origin. */
    baseUrl: string;
    /** Settings endpoint path (GET + POST). */
    settingsPath: string;
    /** Plain name used in the heading and notes. */
    deviceName: string;
    /**
     * Client-side sanity cap for host/uri, matching `LOG_HOST_LEN - 1`. The
     * device remains the authority; this only rejects obvious mistakes early.
     */
    maxHostLength: number;
    formHost: string;
    formUri: string;
    formPort: string;
    formEnabled: boolean;
    loading: boolean;
    saving: boolean;
    notice: Notice | null;
    loaded: boolean;
    private _loadStarted;
    connectedCallback(): void;
    firstUpdated(): void;
    render(): TemplateResult;
    private renderNotice;
    private createClient;
    private load;
    private applySettings;
    private handleHostInput;
    private handleUriInput;
    private handlePortInput;
    private handleEnabledChange;
    /** Returns the trimmed settings, or an inline error message. */
    private validate;
    private handleSave;
}
export {};
