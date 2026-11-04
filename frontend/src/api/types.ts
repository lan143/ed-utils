// Result types for the ed-utils network-logger settings REST API.

/** Current logger target and enabled flag, as served by the device. */
export interface LogSettings {
  host: string;
  uri: string;
  port: number;
  enabled: boolean;
}

/** Successful result carrying the response payload. */
export interface ApiSuccess<T> {
  ok: true;
  data: T;
}

/** Failure side of every API call. Errors are always returned, never thrown. */
export interface ApiFailure {
  ok: false;
  /** Present for HTTP-level failures; absent for network/transport failures. */
  status?: number;
  message: string;
}

export type ApiResult<T> = ApiSuccess<T> | ApiFailure;
