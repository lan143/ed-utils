import { afterEach, describe, expect, it, vi } from 'vitest';
import {
  EduLogSettingsPage,
  type LogSettingsSavedDetail,
} from './log-settings-page';
import type { LogSettings } from '../api/types';

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

function mockFetch(
  impl: (input: RequestInfo | URL, init?: RequestInit) => Promise<Response>,
) {
  const fn = vi.fn(impl);
  vi.stubGlobal('fetch', fn);
  return fn;
}

function mount(attributes: Record<string, string> = {}): EduLogSettingsPage {
  const element = document.createElement(
    'edu-log-settings-page',
  ) as EduLogSettingsPage;
  for (const [name, value] of Object.entries(attributes)) {
    element.setAttribute(name, value);
  }
  document.body.append(element);
  return element;
}

function input(
  element: EduLogSettingsPage,
  id: string,
): HTMLInputElement {
  const found = element.shadowRoot?.querySelector<HTMLInputElement>(`#${id}`);
  if (!found) {
    throw new Error(`missing input: ${id}`);
  }
  return found;
}

function button(
  element: EduLogSettingsPage,
  action: string,
): HTMLButtonElement {
  const found = element.shadowRoot?.querySelector<HTMLButtonElement>(
    `button[data-action="${action}"]`,
  );
  if (!found) {
    throw new Error(`missing button: ${action}`);
  }
  return found;
}

function setText(
  element: EduLogSettingsPage,
  id: string,
  value: string,
): void {
  const field = input(element, id);
  field.value = value;
  field.dispatchEvent(new Event('input', { bubbles: true }));
}

function setChecked(
  element: EduLogSettingsPage,
  id: string,
  checked: boolean,
): void {
  const field = input(element, id);
  field.checked = checked;
  field.dispatchEvent(new Event('change', { bubbles: true }));
}

afterEach(() => {
  vi.unstubAllGlobals();
  document.body.innerHTML = '';
});

describe('<edu-log-settings-page> registration and loading', () => {
  it('registers the element and loads settings on connect', async () => {
    expect(customElements.get('edu-log-settings-page')).toBe(EduLogSettingsPage);
    const fetchMock = mockFetch(async () => jsonResponse(SETTINGS));

    const element = mount();
    await vi.waitFor(() => expect(element.loaded).toBe(true));
    await element.updateComplete;

    expect(fetchMock.mock.calls[0][0]).toBe('/api/log/settings');
    expect(fetchMock.mock.calls[0][1]?.headers).toMatchObject({
      Accept: 'application/json',
    });
    expect(input(element, 'edu-log-host').value).toBe('192.168.1.10');
    expect(input(element, 'edu-log-uri').value).toBe('/logs');
    expect(input(element, 'edu-log-port').value).toBe('8080');
    expect(input(element, 'edu-log-enabled').checked).toBe(true);
  });

  it('respects attribute overrides and reloads via the Reload button', async () => {
    const fetchMock = mockFetch(async () => jsonResponse(SETTINGS));
    const element = mount({
      'base-url': 'http://device.local',
      'settings-path': '/custom/settings',
      'device-name': 'Test Logger',
      'max-host-length': '31',
    });
    await vi.waitFor(() => expect(element.loaded).toBe(true));
    await element.updateComplete;

    expect(element.baseUrl).toBe('http://device.local');
    expect(element.settingsPath).toBe('/custom/settings');
    expect(element.deviceName).toBe('Test Logger');
    expect(element.maxHostLength).toBe(31);
    expect(element.shadowRoot?.textContent).toContain('Test Logger settings');
    expect(fetchMock.mock.calls[0][0]).toBe(
      'http://device.local/custom/settings',
    );

    button(element, 'reload').click();
    await vi.waitFor(() => expect(fetchMock).toHaveBeenCalledTimes(2));
    expect(fetchMock.mock.calls[1][0]).toBe(
      'http://device.local/custom/settings',
    );
  });

  it('shows a load failure in the error banner', async () => {
    mockFetch(async () => jsonResponse({ message: 'invalid settings' }, 422));
    const element = mount();
    await vi.waitFor(() => expect(element.notice).not.toBeNull());
    await element.updateComplete;

    expect(element.notice).toEqual({
      kind: 'error',
      text: 'invalid settings',
    });
    expect(element.shadowRoot?.querySelector('[role="alert"]')?.textContent).toContain(
      'invalid settings',
    );
  });
});

describe('<edu-log-settings-page> validation', () => {
  it('rejects an out-of-range port before any POST', async () => {
    const fetchMock = mockFetch(async () => jsonResponse(SETTINGS));
    const element = mount();
    await vi.waitFor(() => expect(element.loaded).toBe(true));
    await element.updateComplete;

    setText(element, 'edu-log-port', '70000');
    button(element, 'save').click();
    await element.updateComplete;

    expect(element.notice).toEqual({
      kind: 'error',
      text: 'Port must be a whole number between 1 and 65535.',
    });
    expect(fetchMock).toHaveBeenCalledTimes(1);
  });

  it('rejects empty and over-long host/uri before any POST', async () => {
    const fetchMock = mockFetch(async () => jsonResponse(SETTINGS));
    const element = mount({ 'max-host-length': '4' });
    await vi.waitFor(() => expect(element.loaded).toBe(true));
    await element.updateComplete;

    setText(element, 'edu-log-host', '   ');
    button(element, 'save').click();
    await element.updateComplete;
    expect(element.notice?.text).toBe('Host is required.');

    setText(element, 'edu-log-host', '12345');
    button(element, 'save').click();
    await element.updateComplete;
    expect(element.notice?.text).toBe('Host must be 4 characters or fewer.');

    setText(element, 'edu-log-host', 'host');
    setText(element, 'edu-log-uri', '');
    button(element, 'save').click();
    await element.updateComplete;
    expect(element.notice?.text).toBe('URI is required.');

    setText(element, 'edu-log-uri', '/toolong');
    button(element, 'save').click();
    await element.updateComplete;
    expect(element.notice?.text).toBe('URI must be 4 characters or fewer.');

    expect(fetchMock).toHaveBeenCalledTimes(1);
  });
});

describe('<edu-log-settings-page> save outcomes', () => {
  it('POSTs trimmed values, shows the success banner and emits the device echo', async () => {
    const echo: LogSettings = {
      host: '10.0.0.2',
      uri: '/new',
      port: 9000,
      enabled: true,
    };
    const fetchMock = mockFetch(async (_input, init) =>
      init?.method === 'POST' ? jsonResponse(echo) : jsonResponse(SETTINGS),
    );
    const element = mount();
    await vi.waitFor(() => expect(element.loaded).toBe(true));
    await element.updateComplete;
    const listener = vi.fn<(event: Event) => void>();
    element.addEventListener('edu-log-saved', listener, true);

    setText(element, 'edu-log-host', '  10.0.0.2  ');
    setText(element, 'edu-log-uri', ' /new ');
    setText(element, 'edu-log-port', '9000');
    setChecked(element, 'edu-log-enabled', false);
    button(element, 'save').click();

    await vi.waitFor(() => expect(listener).toHaveBeenCalledTimes(1));
    await element.updateComplete;

    const postCall = fetchMock.mock.calls.find(
      ([, init]) => init?.method === 'POST',
    );
    expect(postCall?.[0]).toBe('/api/log/settings');
    expect(postCall?.[1]?.headers).toMatchObject({
      Accept: 'application/json',
      'Content-Type': 'application/json',
    });
    expect(postCall?.[1]?.body).toBe(
      JSON.stringify({
        host: '10.0.0.2',
        uri: '/new',
        port: 9000,
        enabled: false,
      }),
    );

    expect(element.notice).toEqual({ kind: 'info', text: 'Settings saved.' });
    expect(element.shadowRoot?.querySelector('[role="status"]')?.textContent).toContain(
      'Settings saved.',
    );

    const event = listener.mock.calls[0][0] as CustomEvent<LogSettingsSavedDetail>;
    expect(event.detail).toEqual(echo);
    expect(event.bubbles).toBe(true);
    expect(event.composed).toBe(true);

    // The device echo wins over what was submitted.
    expect(element.formHost).toBe('10.0.0.2');
    expect(element.formUri).toBe('/new');
    expect(element.formPort).toBe('9000');
    expect(element.formEnabled).toBe(true);
    expect(element.saving).toBe(false);
  });

  it('shows the device 422 message and does not emit the event', async () => {
    mockFetch(async (_input, init) =>
      init?.method === 'POST'
        ? jsonResponse({ message: 'invalid port' }, 422)
        : jsonResponse(SETTINGS),
    );
    const element = mount();
    await vi.waitFor(() => expect(element.loaded).toBe(true));
    await element.updateComplete;
    const listener = vi.fn();
    element.addEventListener('edu-log-saved', listener);

    setText(element, 'edu-log-port', '9000');
    button(element, 'save').click();
    await vi.waitFor(() =>
      expect(element.notice?.text).toBe('invalid port'),
    );
    await element.updateComplete;

    expect(element.notice).toEqual({ kind: 'error', text: 'invalid port' });
    expect(element.shadowRoot?.querySelector('[role="alert"]')?.textContent).toContain(
      'invalid port',
    );
    expect(listener).not.toHaveBeenCalled();
  });

  it('disables Save and Reload while the POST is in flight', async () => {
    let resolvePost!: (response: Response) => void;
    const fetchMock = mockFetch(async (_input, init) => {
      if (init?.method === 'POST') {
        return new Promise<Response>((resolve) => {
          resolvePost = resolve;
        });
      }
      return jsonResponse(SETTINGS);
    });
    const element = mount();
    await vi.waitFor(() => expect(element.loaded).toBe(true));
    await element.updateComplete;

    button(element, 'save').click();
    await vi.waitFor(() => expect(fetchMock).toHaveBeenCalledTimes(2));
    await element.updateComplete;

    expect(element.saving).toBe(true);
    expect(button(element, 'save').disabled).toBe(true);
    expect(button(element, 'reload').disabled).toBe(true);

    resolvePost(jsonResponse(SETTINGS));
    await vi.waitFor(() => expect(element.saving).toBe(false));
    await element.updateComplete;

    expect(button(element, 'save').disabled).toBe(false);
    expect(button(element, 'reload').disabled).toBe(false);
  });
});
