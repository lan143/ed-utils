import { afterEach, describe, expect, it, vi } from 'vitest';
import { createLogSettingsApiClient } from './client';
import type { LogSettings } from './types';

const SETTINGS: LogSettings = {
  host: '192.168.1.10',
  uri: '/logs',
  port: 8080,
  enabled: true,
};

function jsonResponse(body: unknown, status = 200): Response {
  return new Response(JSON.stringify(body), {
    status,
    headers: { 'Content-Type': 'application/json' },
  });
}

function textResponse(body: string, status = 200): Response {
  return new Response(body, {
    status,
    headers: { 'Content-Type': 'text/plain' },
  });
}

function mockFetch(
  impl: (input: RequestInfo | URL, init?: RequestInit) => Promise<Response>,
) {
  const fn = vi.fn(impl);
  vi.stubGlobal('fetch', fn);
  return fn;
}

afterEach(() => {
  vi.unstubAllGlobals();
});

describe('createLogSettingsApiClient getSettings', () => {
  it('GETs the default path with Accept json and returns the settings', async () => {
    const fetchMock = mockFetch(async () => jsonResponse(SETTINGS));

    const result = await createLogSettingsApiClient().getSettings();

    expect(result).toEqual({ ok: true, data: SETTINGS });
    const [url, init] = fetchMock.mock.calls[0];
    expect(url).toBe('/api/log/settings');
    expect(init?.method).toBeUndefined();
    expect(init?.headers).toMatchObject({ Accept: 'application/json' });
  });

  it('parses a 422 {"message"} body', async () => {
    mockFetch(async () =>
      jsonResponse({ message: 'invalid settings' }, 422),
    );

    const result = await createLogSettingsApiClient().getSettings();

    expect(result).toEqual({
      ok: false,
      status: 422,
      message: 'invalid settings',
    });
  });

  it('falls back when a non-ok body is not JSON', async () => {
    mockFetch(async () => textResponse('<html>bad gateway</html>', 500));

    const result = await createLogSettingsApiClient().getSettings();

    expect(result).toEqual({
      ok: false,
      status: 500,
      message: 'Could not load the logger settings from the device.',
    });
  });

  it('maps a fetch rejection to a network failure with no status', async () => {
    mockFetch(async () => {
      throw new TypeError('Failed to fetch');
    });

    const result = await createLogSettingsApiClient().getSettings();

    expect(result.ok).toBe(false);
    if (!result.ok) {
      expect(result.status).toBeUndefined();
      expect(result.message).toMatch(/network/i);
    }
  });

  it('maps an aborted request to the abort message', async () => {
    const controller = new AbortController();
    controller.abort();
    mockFetch(async () => {
      throw new DOMException('The operation was aborted.', 'AbortError');
    });

    const result = await createLogSettingsApiClient().getSettings(
      controller.signal,
    );

    expect(result).toEqual({ ok: false, message: 'Request aborted.' });
  });

  it('returns an invalid-response failure for a malformed payload', async () => {
    mockFetch(async () =>
      jsonResponse({ host: 'h', uri: '/u', port: '8080', enabled: true }),
    );

    const result = await createLogSettingsApiClient().getSettings();

    expect(result).toEqual({
      ok: false,
      status: 200,
      message: 'Invalid settings response.',
    });
  });

  it('returns an invalid-response failure for a non-JSON success body', async () => {
    mockFetch(async () => textResponse('not json'));

    const result = await createLogSettingsApiClient().getSettings();

    expect(result).toEqual({
      ok: false,
      status: 200,
      message: 'Invalid settings response.',
    });
  });

  it('honours baseUrl, a custom path and passthrough headers', async () => {
    const fetchMock = mockFetch(async () => jsonResponse(SETTINGS));

    const client = createLogSettingsApiClient({
      baseUrl: 'http://device.local',
      settingsPath: '/custom/settings',
      headers: { 'X-Token': 'abc' },
    });
    await client.getSettings();

    const [url, init] = fetchMock.mock.calls[0];
    expect(url).toBe('http://device.local/custom/settings');
    expect(init?.headers).toMatchObject({
      Accept: 'application/json',
      'X-Token': 'abc',
    });
  });
});

describe('createLogSettingsApiClient saveSettings', () => {
  it('POSTs the exact JSON body and returns the device echo', async () => {
    const fetchMock = mockFetch(async () => jsonResponse(SETTINGS));

    const result = await createLogSettingsApiClient().saveSettings(SETTINGS);

    expect(result).toEqual({ ok: true, data: SETTINGS });
    const [url, init] = fetchMock.mock.calls[0];
    expect(url).toBe('/api/log/settings');
    expect(init?.method).toBe('POST');
    expect(init?.headers).toMatchObject({
      Accept: 'application/json',
      'Content-Type': 'application/json',
    });
    expect(init?.body).toBe(JSON.stringify(SETTINGS));
  });

  it('surfaces the server {"message"} on a rejected save', async () => {
    mockFetch(async () => jsonResponse({ message: 'invalid port' }, 422));

    const result = await createLogSettingsApiClient().saveSettings(SETTINGS);

    expect(result).toEqual({
      ok: false,
      status: 422,
      message: 'invalid port',
    });
  });

  it('falls back when a rejected save body is not JSON', async () => {
    mockFetch(async () => textResponse('no', 400));

    const result = await createLogSettingsApiClient().saveSettings(SETTINGS);

    expect(result).toEqual({
      ok: false,
      status: 400,
      message: 'The device rejected the logger settings.',
    });
  });

  it('maps a save transport failure to a network message', async () => {
    mockFetch(async () => {
      throw new TypeError('Failed to fetch');
    });

    const result = await createLogSettingsApiClient().saveSettings(SETTINGS);

    expect(result.ok).toBe(false);
    if (!result.ok) {
      expect(result.status).toBeUndefined();
      expect(result.message).toMatch(/network/i);
    }
  });

  it('maps an aborted save to the abort message', async () => {
    const controller = new AbortController();
    controller.abort();
    mockFetch(async () => {
      throw new DOMException('The operation was aborted.', 'AbortError');
    });

    const result = await createLogSettingsApiClient().saveSettings(
      SETTINGS,
      controller.signal,
    );

    expect(result).toEqual({ ok: false, message: 'Request aborted.' });
  });

  it('returns an invalid-response failure when the echo is malformed', async () => {
    mockFetch(async () => jsonResponse({ host: 'h' }));

    const result = await createLogSettingsApiClient().saveSettings(SETTINGS);

    expect(result).toEqual({
      ok: false,
      status: 200,
      message: 'Invalid settings response.',
    });
  });

  it('honours baseUrl, a custom path and passthrough headers', async () => {
    const fetchMock = mockFetch(async () => jsonResponse(SETTINGS));

    const client = createLogSettingsApiClient({
      baseUrl: 'http://device.local',
      settingsPath: '/custom/settings',
      headers: { 'X-Token': 'abc' },
    });
    await client.saveSettings(SETTINGS);

    const [url, init] = fetchMock.mock.calls[0];
    expect(url).toBe('http://device.local/custom/settings');
    expect(init?.headers).toMatchObject({
      'Content-Type': 'application/json',
      'X-Token': 'abc',
    });
  });
});
