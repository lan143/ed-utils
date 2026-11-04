import type { ApiFailure, ApiResult, LogSettings } from './types';

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
  saveSettings(
    settings: LogSettings,
    signal?: AbortSignal,
  ): Promise<ApiResult<LogSettings>>;
}

const NETWORK_ERROR_MESSAGE =
  'Network request failed. Check the device connection and try again.';
const ABORTED_MESSAGE = 'Request aborted.';
const INVALID_RESPONSE_MESSAGE = 'Invalid settings response.';
const LOAD_FALLBACK = 'Could not load the logger settings from the device.';
const SAVE_FALLBACK = 'The device rejected the logger settings.';

function messageFromErrorBody(body: unknown): string | undefined {
  if (body && typeof body === 'object' && 'message' in body) {
    const value = (body as { message?: unknown }).message;
    if (typeof value === 'string' && value.length > 0) {
      return value;
    }
  }
  return undefined;
}

function isAbort(error: unknown, signal?: AbortSignal): boolean {
  return (
    signal?.aborted === true ||
    (typeof error === 'object' &&
      error !== null &&
      (error as { name?: string }).name === 'AbortError')
  );
}

function transportFailure(error: unknown, signal?: AbortSignal): ApiFailure {
  // An aborted request is its own outcome: no HTTP status, distinct message.
  if (isAbort(error, signal)) {
    return { ok: false, message: ABORTED_MESSAGE };
  }
  return { ok: false, message: NETWORK_ERROR_MESSAGE };
}

async function failureFromResponse(
  response: Response,
  fallback: string,
): Promise<ApiFailure> {
  let message = fallback;
  try {
    const text = await response.text();
    if (text.length > 0) {
      try {
        message = messageFromErrorBody(JSON.parse(text)) ?? message;
      } catch {
        // Not JSON; keep the caller-supplied fallback.
      }
    }
  } catch {
    // Reading the body failed; keep the fallback.
  }
  return { ok: false, status: response.status, message };
}

/**
 * Normalise an untrusted device payload. A malformed reply must become an
 * ApiFailure, never a thrown error or a partially-typed object.
 */
function normalizeSettings(body: unknown): LogSettings | undefined {
  if (!body || typeof body !== 'object') {
    return undefined;
  }
  const { host, uri, port, enabled } = body as Record<string, unknown>;
  if (typeof host !== 'string') {
    return undefined;
  }
  if (typeof uri !== 'string') {
    return undefined;
  }
  if (typeof port !== 'number' || !Number.isFinite(port)) {
    return undefined;
  }
  if (typeof enabled !== 'boolean') {
    return undefined;
  }
  return { host, uri, port, enabled };
}

async function successFromResponse(
  response: Response,
): Promise<ApiResult<LogSettings>> {
  let body: unknown;
  try {
    body = await response.json();
  } catch {
    return {
      ok: false,
      status: response.status,
      message: INVALID_RESPONSE_MESSAGE,
    };
  }

  const settings = normalizeSettings(body);
  if (!settings) {
    return {
      ok: false,
      status: response.status,
      message: INVALID_RESPONSE_MESSAGE,
    };
  }
  return { ok: true, data: settings };
}

async function load(
  url: string,
  headers: Record<string, string>,
  signal?: AbortSignal,
): Promise<ApiResult<LogSettings>> {
  let response: Response;
  try {
    response = await fetch(url, {
      headers: { Accept: 'application/json', ...headers },
      signal,
    });
  } catch (error) {
    return transportFailure(error, signal);
  }

  if (!response.ok) {
    return failureFromResponse(response, LOAD_FALLBACK);
  }
  return successFromResponse(response);
}

async function save(
  url: string,
  settings: LogSettings,
  headers: Record<string, string>,
  signal?: AbortSignal,
): Promise<ApiResult<LogSettings>> {
  let response: Response;
  try {
    response = await fetch(url, {
      method: 'POST',
      headers: {
        Accept: 'application/json',
        'Content-Type': 'application/json',
        ...headers,
      },
      body: JSON.stringify(settings),
      signal,
    });
  } catch (error) {
    return transportFailure(error, signal);
  }

  if (!response.ok) {
    return failureFromResponse(response, SAVE_FALLBACK);
  }
  return successFromResponse(response);
}

export function createLogSettingsApiClient(
  options: LogSettingsApiClientOptions = {},
): LogSettingsApiClient {
  const baseUrl = options.baseUrl ?? '';
  const settingsPath = options.settingsPath ?? '/api/log/settings';
  const extraHeaders = options.headers ?? {};

  const settingsUrl = `${baseUrl}${settingsPath}`;

  return {
    getSettings(signal?: AbortSignal): Promise<ApiResult<LogSettings>> {
      return load(settingsUrl, extraHeaders, signal);
    },

    saveSettings(
      settings: LogSettings,
      signal?: AbortSignal,
    ): Promise<ApiResult<LogSettings>> {
      return save(settingsUrl, settings, extraHeaders, signal);
    },
  };
}
