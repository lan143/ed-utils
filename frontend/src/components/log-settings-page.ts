import { LitElement, css, html, nothing } from 'lit';
import type { TemplateResult } from 'lit';
import { createLogSettingsApiClient } from '../api/client';
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
export class EduLogSettingsPage extends LitElement {
  static override properties = {
    baseUrl: { type: String, attribute: 'base-url' },
    settingsPath: { type: String, attribute: 'settings-path' },
    deviceName: { type: String, attribute: 'device-name' },
    maxHostLength: { type: Number, attribute: 'max-host-length' },
    formHost: { state: true },
    formUri: { state: true },
    formPort: { state: true },
    formEnabled: { state: true },
    loading: { state: true },
    saving: { state: true },
    notice: { state: true },
    loaded: { state: true },
  };

  static override styles = css`
    :host {
      display: block;
      font-family: system-ui, -apple-system, 'Segoe UI', Roboto, sans-serif;
      color: #1f2430;
      --_accent: var(--edu-accent, #2563eb);
      --_danger: var(--edu-danger, #dc2626);
      --_muted: var(--edu-muted, #6b7280);
      --_border: #e2e5ea;
      --_surface: #ffffff;
    }

    .stack {
      display: grid;
      gap: 1rem;
      max-width: 34rem;
    }

    .card {
      background: var(--_surface);
      border: 1px solid var(--_border);
      border-radius: 10px;
      padding: 1rem 1.25rem;
      box-shadow: 0 1px 2px rgba(16, 24, 40, 0.05);
    }

    h2 {
      font-size: 1rem;
      margin: 0 0 0.5rem;
    }

    p {
      margin: 0 0 0.75rem;
      font-size: 0.875rem;
      line-height: 1.45;
    }

    .muted {
      color: var(--_muted);
    }

    .field {
      display: grid;
      gap: 0.35rem;
      margin-bottom: 0.75rem;
    }

    .field-label {
      font-size: 0.75rem;
      text-transform: uppercase;
      letter-spacing: 0.04em;
      color: var(--_muted);
    }

    input[type='text'],
    input[type='number'] {
      font: inherit;
      font-size: 0.875rem;
      padding: 0.45rem 0.6rem;
      border: 1px solid var(--_border);
      border-radius: 8px;
      background: var(--_surface);
      color: inherit;
      max-width: 100%;
    }

    input:focus {
      outline: 2px solid var(--_accent);
      outline-offset: 1px;
    }

    .checkbox-row {
      display: flex;
      align-items: center;
      gap: 0.5rem;
      font-size: 0.875rem;
      margin-bottom: 0.75rem;
    }

    .form-actions {
      display: flex;
      flex-wrap: wrap;
      gap: 0.5rem;
    }

    button {
      font: inherit;
      font-size: 0.85rem;
      padding: 0.45rem 0.85rem;
      border-radius: 8px;
      border: 1px solid var(--_border);
      background: var(--_surface);
      color: inherit;
      cursor: pointer;
    }

    button:hover:not(:disabled) {
      border-color: var(--_accent);
    }

    button.primary {
      background: var(--_accent);
      border-color: var(--_accent);
      color: #ffffff;
    }

    button:disabled {
      cursor: not-allowed;
      opacity: 0.55;
    }

    .banner {
      margin: 0 0 0.75rem;
      padding: 0.5rem 0.75rem;
      border-radius: 8px;
      font-size: 0.85rem;
    }

    .banner-error {
      background: #fef2f2;
      border: 1px solid #fecaca;
      color: var(--_danger);
    }

    .banner-info {
      background: #eff6ff;
      border: 1px solid #bfdbfe;
      color: #1d4ed8;
    }
  `;

  /** Prefix for every API request. Default '' = same origin. */
  baseUrl = '';

  /** Settings endpoint path (GET + POST). */
  settingsPath = '/api/log/settings';

  /** Plain name used in the heading and notes. */
  deviceName = 'logger';

  /**
   * Client-side sanity cap for host/uri, matching `LOG_HOST_LEN - 1`. The
   * device remains the authority; this only rejects obvious mistakes early.
   */
  maxHostLength = 63;

  formHost = '';

  formUri = '';

  formPort = '';

  formEnabled = false;

  loading = false;

  saving = false;

  notice: Notice | null = null;

  loaded = false;

  private _loadStarted = false;

  override connectedCallback(): void {
    super.connectedCallback();
    void this.load();
  }

  override firstUpdated(): void {
    // connectedCallback normally starts the load; this covers late upgrades.
    if (!this._loadStarted) {
      void this.load();
    }
  }

  override render(): TemplateResult {
    return html`
      <div class="stack">
        <section class="card" aria-labelledby="edu-log-heading">
          <h2 id="edu-log-heading">${this.deviceName} settings</h2>
          <p class="muted">
            Configure the network logger on the ${this.deviceName}. Changes apply
            immediately; persistence depends on the device firmware.
          </p>
          ${this.renderNotice(this.notice)}
          <label class="field" for="edu-log-host">
            <span class="field-label">Host</span>
            <input
              id="edu-log-host"
              type="text"
              .value=${this.formHost}
              maxlength=${this.maxHostLength}
              ?disabled=${this.loading || this.saving}
              @input=${(event: Event) => this.handleHostInput(event)}
            />
          </label>
          <label class="field" for="edu-log-uri">
            <span class="field-label">URI</span>
            <input
              id="edu-log-uri"
              type="text"
              .value=${this.formUri}
              maxlength=${this.maxHostLength}
              ?disabled=${this.loading || this.saving}
              @input=${(event: Event) => this.handleUriInput(event)}
            />
          </label>
          <label class="field" for="edu-log-port">
            <span class="field-label">Port</span>
            <input
              id="edu-log-port"
              type="number"
              min="1"
              max="65535"
              inputmode="numeric"
              .value=${this.formPort}
              ?disabled=${this.loading || this.saving}
              @input=${(event: Event) => this.handlePortInput(event)}
            />
          </label>
          <label class="checkbox-row" for="edu-log-enabled">
            <input
              id="edu-log-enabled"
              type="checkbox"
              .checked=${this.formEnabled}
              ?disabled=${this.loading || this.saving}
              @change=${(event: Event) => this.handleEnabledChange(event)}
            />
            <span>Enabled</span>
          </label>
          <div class="form-actions">
            <button
              type="button"
              class="primary"
              data-action="save"
              ?disabled=${this.loading || this.saving}
              @click=${() => void this.handleSave()}
            >
              ${this.saving ? 'Saving…' : 'Save'}
            </button>
            <button
              type="button"
              data-action="reload"
              ?disabled=${this.loading || this.saving}
              @click=${() => void this.load()}
            >
              ${this.loading ? 'Loading…' : 'Reload'}
            </button>
          </div>
        </section>
      </div>
    `;
  }

  private renderNotice(notice: Notice | null): TemplateResult | typeof nothing {
    if (!notice) {
      return nothing;
    }
    return html`<div
      class="banner banner-${notice.kind}"
      role=${notice.kind === 'error' ? 'alert' : 'status'}
    >
      ${notice.text}
    </div>`;
  }

  private createClient() {
    return createLogSettingsApiClient({
      baseUrl: this.baseUrl,
      settingsPath: this.settingsPath,
    });
  }

  private async load(): Promise<void> {
    if (this.loading || this.saving) {
      return;
    }

    this._loadStarted = true;
    this.loading = true;
    this.notice = null;
    try {
      const result = await this.createClient().getSettings();
      if (!result.ok) {
        this.notice = { kind: 'error', text: result.message };
        return;
      }
      this.applySettings(result.data);
      this.loaded = true;
    } finally {
      this.loading = false;
    }
  }

  private applySettings(settings: LogSettings): void {
    this.formHost = settings.host;
    this.formUri = settings.uri;
    this.formPort = String(settings.port);
    this.formEnabled = settings.enabled;
  }

  private handleHostInput(event: Event): void {
    this.formHost = (event.currentTarget as HTMLInputElement).value;
  }

  private handleUriInput(event: Event): void {
    this.formUri = (event.currentTarget as HTMLInputElement).value;
  }

  private handlePortInput(event: Event): void {
    this.formPort = (event.currentTarget as HTMLInputElement).value;
  }

  private handleEnabledChange(event: Event): void {
    this.formEnabled = (event.currentTarget as HTMLInputElement).checked;
  }

  /** Returns the trimmed settings, or an inline error message. */
  private validate(): LogSettings | string {
    const host = this.formHost.trim();
    const uri = this.formUri.trim();

    if (host.length === 0) {
      return 'Host is required.';
    }
    if (host.length > this.maxHostLength) {
      return `Host must be ${this.maxHostLength} characters or fewer.`;
    }
    if (uri.length === 0) {
      return 'URI is required.';
    }
    if (uri.length > this.maxHostLength) {
      return `URI must be ${this.maxHostLength} characters or fewer.`;
    }

    const portText = this.formPort.trim();
    if (!/^\d+$/.test(portText)) {
      return 'Port must be a whole number between 1 and 65535.';
    }
    const port = Number(portText);
    if (!Number.isInteger(port) || port < 1 || port > 65535) {
      return 'Port must be a whole number between 1 and 65535.';
    }

    return { host, uri, port, enabled: this.formEnabled };
  }

  private async handleSave(): Promise<void> {
    if (this.loading || this.saving) {
      return;
    }

    const settings = this.validate();
    if (typeof settings === 'string') {
      this.notice = { kind: 'error', text: settings };
      return;
    }

    this.saving = true;
    this.notice = null;
    try {
      const result = await this.createClient().saveSettings(settings);
      if (!result.ok) {
        this.notice = { kind: 'error', text: result.message };
        return;
      }

      // The device echo is the current state; reflect it back into the form.
      this.applySettings(result.data);
      this.loaded = true;
      this.notice = { kind: 'info', text: 'Settings saved.' };

      this.dispatchEvent(
        new CustomEvent<LogSettingsSavedDetail>('edu-log-saved', {
          detail: result.data,
          bubbles: true,
          composed: true,
        }),
      );
    } finally {
      this.saving = false;
    }
  }
}

if (
  typeof customElements !== 'undefined' &&
  !customElements.get('edu-log-settings-page')
) {
  customElements.define('edu-log-settings-page', EduLogSettingsPage);
}
