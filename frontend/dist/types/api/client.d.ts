import type { ApiResult, LogSettings } from './types';
export interface LogSettingsApiClientOptions {
    /** Prefix for every request path. Default '' = same origin. */
    baseUrl?: string;
    /** Settings endpoint path (GET + POST). Default '/api/log/settings'. */
    settingsPath?: string;
    /** Extra request headers, merged over the defaults. */
    headers?: Record<string, string>;
}
export interface LogSettingsApiClient {
    getSettings(signal?: AbortSignal): Promise<ApiResult<LogSettings>>;
    saveSettings(settings: LogSettings, signal?: AbortSignal): Promise<ApiResult<LogSettings>>;
}
export declare function createLogSettingsApiClient(options?: LogSettingsApiClientOptions): LogSettingsApiClient;
