// src/api/client.ts
var e = "Invalid settings response.";
function t(e) {
	if (e && typeof e == "object" && "message" in e) {
		let t = e.message;
		if (typeof t == "string" && t.length > 0) return t;
	}
}
function n(e, t) {
	return t?.aborted === !0 || typeof e == "object" && !!e && e.name === "AbortError";
}
function r(e, t) {
	return n(e, t) ? {
		ok: !1,
		message: "Request aborted."
	} : {
		ok: !1,
		message: "Network request failed. Check the device connection and try again."
	};
}
async function i(e, n) {
	let r = n;
	try {
		let n = await e.text();
		if (n.length > 0) try {
			r = t(JSON.parse(n)) ?? r;
		} catch {}
	} catch {}
	return {
		ok: !1,
		status: e.status,
		message: r
	};
}
function a(e) {
	if (!e || typeof e != "object") return;
	let { host: t, uri: n, port: r, enabled: i } = e;
	if (typeof t == "string" && typeof n == "string" && typeof r == "number" && Number.isFinite(r) && typeof i == "boolean") return {
		host: t,
		uri: n,
		port: r,
		enabled: i
	};
}
async function o(t) {
	let n;
	try {
		n = await t.json();
	} catch {
		return {
			ok: !1,
			status: t.status,
			message: e
		};
	}
	let r = a(n);
	return r ? {
		ok: !0,
		data: r
	} : {
		ok: !1,
		status: t.status,
		message: e
	};
}
async function s(e, t, n) {
	let a;
	try {
		a = await fetch(e, {
			headers: {
				Accept: "application/json",
				...t
			},
			signal: n
		});
	} catch (e) {
		return r(e, n);
	}
	return a.ok ? o(a) : i(a, "Could not load the logger settings from the device.");
}
async function c(e, t, n, a) {
	let s;
	try {
		s = await fetch(e, {
			method: "POST",
			headers: {
				Accept: "application/json",
				"Content-Type": "application/json",
				...n
			},
			body: JSON.stringify(t),
			signal: a
		});
	} catch (e) {
		return r(e, a);
	}
	return s.ok ? o(s) : i(s, "The device rejected the logger settings.");
}
function l(e = {}) {
	let t = e.baseUrl ?? "", n = e.settingsPath ?? "/api/log/settings", r = e.headers ?? {}, i = `${t}${n}`;
	return {
		getSettings(e) {
			return s(i, r, e);
		},
		saveSettings(e, t) {
			return c(i, e, r, t);
		}
	};
}
// node_modules/@lit/reactive-element/css-tag.js
var u = globalThis, d = u.ShadowRoot && (u.ShadyCSS === void 0 || u.ShadyCSS.nativeShadow) && "adoptedStyleSheets" in Document.prototype && "replace" in CSSStyleSheet.prototype, f = Symbol(), p = /* @__PURE__ */ new WeakMap(), m = class {
	constructor(e, t, n) {
		if (this._$cssResult$ = !0, n !== f) throw Error("CSSResult is not constructable. Use `unsafeCSS` or `css` instead.");
		this.cssText = e, this.t = t;
	}
	get styleSheet() {
		let e = this.o, t = this.t;
		if (d && e === void 0) {
			let n = t !== void 0 && t.length === 1;
			n && (e = p.get(t)), e === void 0 && ((this.o = e = new CSSStyleSheet()).replaceSync(this.cssText), n && p.set(t, e));
		}
		return e;
	}
	toString() {
		return this.cssText;
	}
}, ee = (e) => new m(typeof e == "string" ? e : e + "", void 0, f), te = (e, ...t) => new m(e.length === 1 ? e[0] : t.reduce((t, n, r) => t + ((e) => {
	if (!0 === e._$cssResult$) return e.cssText;
	if (typeof e == "number") return e;
	throw Error("Value passed to 'css' function must be a 'css' function result: " + e + ". Use 'unsafeCSS' to pass non-literal values, but take care to ensure page security.");
})(n) + e[r + 1], e[0]), e, f), ne = (e, t) => {
	if (d) e.adoptedStyleSheets = t.map((e) => e instanceof CSSStyleSheet ? e : e.styleSheet);
	else for (let n of t) {
		let t = document.createElement("style"), r = u.litNonce;
		r !== void 0 && t.setAttribute("nonce", r), t.textContent = n.cssText, e.appendChild(t);
	}
}, h = d ? (e) => e : (e) => e instanceof CSSStyleSheet ? ((e) => {
	let t = "";
	for (let n of e.cssRules) t += n.cssText;
	return ee(t);
})(e) : e, { is: re, defineProperty: ie, getOwnPropertyDescriptor: ae, getOwnPropertyNames: oe, getOwnPropertySymbols: se, getPrototypeOf: ce } = Object, g = globalThis, _ = g.trustedTypes, le = _ ? _.emptyScript : "", ue = g.reactiveElementPolyfillSupport, v = (e, t) => e, y = {
	toAttribute(e, t) {
		switch (t) {
			case Boolean:
				e = e ? le : null;
				break;
			case Object:
			case Array: e = e == null ? e : JSON.stringify(e);
		}
		return e;
	},
	fromAttribute(e, t) {
		let n = e;
		switch (t) {
			case Boolean:
				n = e !== null;
				break;
			case Number:
				n = e === null ? null : Number(e);
				break;
			case Object:
			case Array: try {
				n = JSON.parse(e);
			} catch {
				n = null;
			}
		}
		return n;
	}
}, b = (e, t) => !re(e, t), x = {
	attribute: !0,
	type: String,
	converter: y,
	reflect: !1,
	useDefault: !1,
	hasChanged: b
};
Symbol.metadata ??= Symbol("metadata"), g.litPropertyMetadata ??= /* @__PURE__ */ new WeakMap();
var S = class extends HTMLElement {
	static addInitializer(e) {
		this._$Ei(), (this.l ??= []).push(e);
	}
	static get observedAttributes() {
		return this.finalize(), this._$Eh && [...this._$Eh.keys()];
	}
	static createProperty(e, t = x) {
		if (t.state && (t.attribute = !1), this._$Ei(), this.prototype.hasOwnProperty(e) && ((t = Object.create(t)).wrapped = !0), this.elementProperties.set(e, t), !t.noAccessor) {
			let n = Symbol(), r = this.getPropertyDescriptor(e, n, t);
			r !== void 0 && ie(this.prototype, e, r);
		}
	}
	static getPropertyDescriptor(e, t, n) {
		let { get: r, set: i } = ae(this.prototype, e) ?? {
			get() {
				return this[t];
			},
			set(e) {
				this[t] = e;
			}
		};
		return {
			get: r,
			set(t) {
				let a = r?.call(this);
				i?.call(this, t), this.requestUpdate(e, a, n);
			},
			configurable: !0,
			enumerable: !0
		};
	}
	static getPropertyOptions(e) {
		return this.elementProperties.get(e) ?? x;
	}
	static _$Ei() {
		if (this.hasOwnProperty(v("elementProperties"))) return;
		let e = ce(this);
		e.finalize(), e.l !== void 0 && (this.l = [...e.l]), this.elementProperties = new Map(e.elementProperties);
	}
	static finalize() {
		if (this.hasOwnProperty(v("finalized"))) return;
		if (this.finalized = !0, this._$Ei(), this.hasOwnProperty(v("properties"))) {
			let e = this.properties, t = [...oe(e), ...se(e)];
			for (let n of t) this.createProperty(n, e[n]);
		}
		let e = this[Symbol.metadata];
		if (e !== null) {
			let t = litPropertyMetadata.get(e);
			if (t !== void 0) for (let [e, n] of t) this.elementProperties.set(e, n);
		}
		this._$Eh = /* @__PURE__ */ new Map();
		for (let [e, t] of this.elementProperties) {
			let n = this._$Eu(e, t);
			n !== void 0 && this._$Eh.set(n, e);
		}
		this.elementStyles = this.finalizeStyles(this.styles);
	}
	static finalizeStyles(e) {
		let t = [];
		if (Array.isArray(e)) {
			let n = new Set(e.flat(1 / 0).reverse());
			for (let e of n) t.unshift(h(e));
		} else e !== void 0 && t.push(h(e));
		return t;
	}
	static _$Eu(e, t) {
		let n = t.attribute;
		return !1 === n ? void 0 : typeof n == "string" ? n : typeof e == "string" ? e.toLowerCase() : void 0;
	}
	constructor() {
		super(), this._$Ep = void 0, this.isUpdatePending = !1, this.hasUpdated = !1, this._$Em = null, this._$Ev();
	}
	_$Ev() {
		this._$ES = new Promise((e) => this.enableUpdating = e), this._$AL = /* @__PURE__ */ new Map(), this._$E_(), this.requestUpdate(), this.constructor.l?.forEach((e) => e(this));
	}
	addController(e) {
		(this._$EO ??= /* @__PURE__ */ new Set()).add(e), this.renderRoot !== void 0 && this.isConnected && e.hostConnected?.();
	}
	removeController(e) {
		this._$EO?.delete(e);
	}
	_$E_() {
		let e = /* @__PURE__ */ new Map(), t = this.constructor.elementProperties;
		for (let n of t.keys()) this.hasOwnProperty(n) && (e.set(n, this[n]), delete this[n]);
		e.size > 0 && (this._$Ep = e);
	}
	createRenderRoot() {
		let e = this.shadowRoot ?? this.attachShadow(this.constructor.shadowRootOptions);
		return ne(e, this.constructor.elementStyles), e;
	}
	connectedCallback() {
		this.renderRoot ??= this.createRenderRoot(), this.enableUpdating(!0), this._$EO?.forEach((e) => e.hostConnected?.());
	}
	enableUpdating(e) {}
	disconnectedCallback() {
		this._$EO?.forEach((e) => e.hostDisconnected?.());
	}
	attributeChangedCallback(e, t, n) {
		this._$AK(e, n);
	}
	_$ET(e, t) {
		let n = this.constructor.elementProperties.get(e), r = this.constructor._$Eu(e, n);
		if (r !== void 0 && !0 === n.reflect) {
			let i = (n.converter?.toAttribute === void 0 ? y : n.converter).toAttribute(t, n.type);
			this._$Em = e, i == null ? this.removeAttribute(r) : this.setAttribute(r, i), this._$Em = null;
		}
	}
	_$AK(e, t) {
		let n = this.constructor, r = n._$Eh.get(e);
		if (r !== void 0 && this._$Em !== r) {
			let e = n.getPropertyOptions(r), i = typeof e.converter == "function" ? { fromAttribute: e.converter } : e.converter?.fromAttribute === void 0 ? y : e.converter;
			this._$Em = r;
			let a = i.fromAttribute(t, e.type);
			this[r] = a ?? this._$Ej?.get(r) ?? a, this._$Em = null;
		}
	}
	requestUpdate(e, t, n, r = !1, i) {
		if (e !== void 0) {
			let a = this.constructor;
			if (!1 === r && (i = this[e]), n ??= a.getPropertyOptions(e), !((n.hasChanged ?? b)(i, t) || n.useDefault && n.reflect && i === this._$Ej?.get(e) && !this.hasAttribute(a._$Eu(e, n)))) return;
			this.C(e, t, n);
		}
		!1 === this.isUpdatePending && (this._$ES = this._$EP());
	}
	C(e, t, { useDefault: n, reflect: r, wrapped: i }, a) {
		n && !(this._$Ej ??= /* @__PURE__ */ new Map()).has(e) && (this._$Ej.set(e, a ?? t ?? this[e]), !0 !== i || a !== void 0) || (this._$AL.has(e) || (this.hasUpdated || n || (t = void 0), this._$AL.set(e, t)), !0 === r && this._$Em !== e && (this._$Eq ??= /* @__PURE__ */ new Set()).add(e));
	}
	async _$EP() {
		this.isUpdatePending = !0;
		try {
			await this._$ES;
		} catch (e) {
			Promise.reject(e);
		}
		let e = this.scheduleUpdate();
		return e != null && await e, !this.isUpdatePending;
	}
	scheduleUpdate() {
		return this.performUpdate();
	}
	performUpdate() {
		if (!this.isUpdatePending) return;
		if (!this.hasUpdated) {
			if (this.renderRoot ??= this.createRenderRoot(), this._$Ep) {
				for (let [e, t] of this._$Ep) this[e] = t;
				this._$Ep = void 0;
			}
			let e = this.constructor.elementProperties;
			if (e.size > 0) for (let [t, n] of e) {
				let { wrapped: e } = n, r = this[t];
				!0 !== e || this._$AL.has(t) || r === void 0 || this.C(t, void 0, n, r);
			}
		}
		let e = !1, t = this._$AL;
		try {
			e = this.shouldUpdate(t), e ? (this.willUpdate(t), this._$EO?.forEach((e) => e.hostUpdate?.()), this.update(t)) : this._$EM();
		} catch (t) {
			throw e = !1, this._$EM(), t;
		}
		e && this._$AE(t);
	}
	willUpdate(e) {}
	_$AE(e) {
		this._$EO?.forEach((e) => e.hostUpdated?.()), this.hasUpdated || (this.hasUpdated = !0, this.firstUpdated(e)), this.updated(e);
	}
	_$EM() {
		this._$AL = /* @__PURE__ */ new Map(), this.isUpdatePending = !1;
	}
	get updateComplete() {
		return this.getUpdateComplete();
	}
	getUpdateComplete() {
		return this._$ES;
	}
	shouldUpdate(e) {
		return !0;
	}
	update(e) {
		this._$Eq &&= this._$Eq.forEach((e) => this._$ET(e, this[e])), this._$EM();
	}
	updated(e) {}
	firstUpdated(e) {}
};
S.elementStyles = [], S.shadowRootOptions = { mode: "open" }, S[v("elementProperties")] = /* @__PURE__ */ new Map(), S[v("finalized")] = /* @__PURE__ */ new Map(), ue?.({ ReactiveElement: S }), (g.reactiveElementVersions ??= []).push("2.1.2");
// node_modules/lit-html/lit-html.js
var C = globalThis, w = (e) => e, T = C.trustedTypes, E = T ? T.createPolicy("lit-html", { createHTML: (e) => e }) : void 0, D = "$lit$", O = `lit$${Math.random().toFixed(9).slice(2)}$`, k = "?" + O, de = `<${k}>`, A = document, j = () => A.createComment(""), M = (e) => e === null || typeof e != "object" && typeof e != "function", N = Array.isArray, fe = (e) => N(e) || typeof e?.[Symbol.iterator] == "function", P = "[ 	\n\f\r]", F = /<(?:(!--|\/[^a-zA-Z])|(\/?[a-zA-Z][^>\s]*)|(\/?$))/g, I = /-->/g, L = />/g, R = RegExp(`>|${P}(?:([^\\s"'>=/]+)(${P}*=${P}*(?:[^ \t\n\f\r"'\`<>=]|("|')|))|$)`, "g"), z = /'/g, B = /"/g, V = /^(?:script|style|textarea|title)$/i, H = ((e) => (t, ...n) => ({
	_$litType$: e,
	strings: t,
	values: n
}))(1), U = Symbol.for("lit-noChange"), W = Symbol.for("lit-nothing"), G = /* @__PURE__ */ new WeakMap(), K = A.createTreeWalker(A, 129);
function q(e, t) {
	if (!N(e) || !e.hasOwnProperty("raw")) throw Error("invalid template strings array");
	return E === void 0 ? t : E.createHTML(t);
}
var pe = (e, t) => {
	let n = e.length - 1, r = [], i, a = t === 2 ? "<svg>" : t === 3 ? "<math>" : "", o = F;
	for (let t = 0; t < n; t++) {
		let n = e[t], s, c, l = -1, u = 0;
		for (; u < n.length && (o.lastIndex = u, c = o.exec(n), c !== null);) u = o.lastIndex, o === F ? c[1] === "!--" ? o = I : c[1] === void 0 ? c[2] === void 0 ? c[3] !== void 0 && (o = R) : (V.test(c[2]) && (i = RegExp("</" + c[2], "g")), o = R) : o = L : o === R ? c[0] === ">" ? (o = i ?? F, l = -1) : c[1] === void 0 ? l = -2 : (l = o.lastIndex - c[2].length, s = c[1], o = c[3] === void 0 ? R : c[3] === "\"" ? B : z) : o === B || o === z ? o = R : o === I || o === L ? o = F : (o = R, i = void 0);
		let d = o === R && e[t + 1].startsWith("/>") ? " " : "";
		a += o === F ? n + de : l >= 0 ? (r.push(s), n.slice(0, l) + D + n.slice(l) + O + d) : n + O + (l === -2 ? t : d);
	}
	return [q(e, a + (e[n] || "<?>") + (t === 2 ? "</svg>" : t === 3 ? "</math>" : "")), r];
}, J = class e {
	constructor({ strings: t, _$litType$: n }, r) {
		let i;
		this.parts = [];
		let a = 0, o = 0, s = t.length - 1, c = this.parts, [l, u] = pe(t, n);
		if (this.el = e.createElement(l, r), K.currentNode = this.el.content, n === 2 || n === 3) {
			let e = this.el.content.firstChild;
			e.replaceWith(...e.childNodes);
		}
		for (; (i = K.nextNode()) !== null && c.length < s;) {
			if (i.nodeType === 1) {
				if (i.hasAttributes()) for (let e of i.getAttributeNames()) if (e.endsWith(D)) {
					let t = u[o++], n = i.getAttribute(e).split(O), r = /([.?@])?(.*)/.exec(t);
					c.push({
						type: 1,
						index: a,
						name: r[2],
						strings: n,
						ctor: r[1] === "." ? he : r[1] === "?" ? ge : r[1] === "@" ? _e : Z
					}), i.removeAttribute(e);
				} else e.startsWith(O) && (c.push({
					type: 6,
					index: a
				}), i.removeAttribute(e));
				if (V.test(i.tagName)) {
					let e = i.textContent.split(O), t = e.length - 1;
					if (t > 0) {
						i.textContent = T ? T.emptyScript : "";
						for (let n = 0; n < t; n++) i.append(e[n], j()), K.nextNode(), c.push({
							type: 2,
							index: ++a
						});
						i.append(e[t], j());
					}
				}
			} else if (i.nodeType === 8) {
				if (i.data === k) c.push({
					type: 2,
					index: a
				});
				else {
					let e = -1;
					for (; (e = i.data.indexOf(O, e + 1)) !== -1;) c.push({
						type: 7,
						index: a
					}), e += O.length - 1;
				}
			}
			a++;
		}
	}
	static createElement(e, t) {
		let n = A.createElement("template");
		return n.innerHTML = e, n;
	}
};
function Y(e, t, n = e, r) {
	if (t === U) return t;
	let i = r === void 0 ? n._$Cl : n._$Co?.[r], a = M(t) ? void 0 : t._$litDirective$;
	return i?.constructor !== a && (i?._$AO?.(!1), a === void 0 ? i = void 0 : (i = new a(e), i._$AT(e, n, r)), r === void 0 ? n._$Cl = i : (n._$Co ??= [])[r] = i), i !== void 0 && (t = Y(e, i._$AS(e, t.values), i, r)), t;
}
var me = class {
	constructor(e, t) {
		this._$AV = [], this._$AN = void 0, this._$AD = e, this._$AM = t;
	}
	get parentNode() {
		return this._$AM.parentNode;
	}
	get _$AU() {
		return this._$AM._$AU;
	}
	u(e) {
		let { el: { content: t }, parts: n } = this._$AD, r = (e?.creationScope ?? A).importNode(t, !0);
		K.currentNode = r;
		let i = K.nextNode(), a = 0, o = 0, s = n[0];
		for (; s !== void 0;) {
			if (a === s.index) {
				let t;
				s.type === 2 ? t = new X(i, i.nextSibling, this, e) : s.type === 1 ? t = new s.ctor(i, s.name, s.strings, this, e) : s.type === 6 && (t = new ve(i, this, e)), this._$AV.push(t), s = n[++o];
			}
			a !== s?.index && (i = K.nextNode(), a++);
		}
		return K.currentNode = A, r;
	}
	p(e) {
		let t = 0;
		for (let n of this._$AV) n !== void 0 && (n.strings === void 0 ? n._$AI(e[t]) : (n._$AI(e, n, t), t += n.strings.length - 2)), t++;
	}
}, X = class e {
	get _$AU() {
		return this._$AM?._$AU ?? this._$Cv;
	}
	constructor(e, t, n, r) {
		this.type = 2, this._$AH = W, this._$AN = void 0, this._$AA = e, this._$AB = t, this._$AM = n, this.options = r, this._$Cv = r?.isConnected ?? !0;
	}
	get parentNode() {
		let e = this._$AA.parentNode, t = this._$AM;
		return t !== void 0 && e?.nodeType === 11 && (e = t.parentNode), e;
	}
	get startNode() {
		return this._$AA;
	}
	get endNode() {
		return this._$AB;
	}
	_$AI(e, t = this) {
		e = Y(this, e, t), M(e) ? e === W || e == null || e === "" ? (this._$AH !== W && this._$AR(), this._$AH = W) : e !== this._$AH && e !== U && this._(e) : e._$litType$ === void 0 ? e.nodeType === void 0 ? fe(e) ? this.k(e) : this._(e) : this.T(e) : this.$(e);
	}
	O(e) {
		return this._$AA.parentNode.insertBefore(e, this._$AB);
	}
	T(e) {
		this._$AH !== e && (this._$AR(), this._$AH = this.O(e));
	}
	_(e) {
		this._$AH !== W && M(this._$AH) ? this._$AA.nextSibling.data = e : this.T(A.createTextNode(e)), this._$AH = e;
	}
	$(e) {
		let { values: t, _$litType$: n } = e, r = typeof n == "number" ? this._$AC(e) : (n.el === void 0 && (n.el = J.createElement(q(n.h, n.h[0]), this.options)), n);
		if (this._$AH?._$AD === r) this._$AH.p(t);
		else {
			let e = new me(r, this), n = e.u(this.options);
			e.p(t), this.T(n), this._$AH = e;
		}
	}
	_$AC(e) {
		let t = G.get(e.strings);
		return t === void 0 && G.set(e.strings, t = new J(e)), t;
	}
	k(t) {
		N(this._$AH) || (this._$AH = [], this._$AR());
		let n = this._$AH, r, i = 0;
		for (let a of t) i === n.length ? n.push(r = new e(this.O(j()), this.O(j()), this, this.options)) : r = n[i], r._$AI(a), i++;
		i < n.length && (this._$AR(r && r._$AB.nextSibling, i), n.length = i);
	}
	_$AR(e = this._$AA.nextSibling, t) {
		for (this._$AP?.(!1, !0, t); e !== this._$AB;) {
			let t = w(e).nextSibling;
			w(e).remove(), e = t;
		}
	}
	setConnected(e) {
		this._$AM === void 0 && (this._$Cv = e, this._$AP?.(e));
	}
}, Z = class {
	get tagName() {
		return this.element.tagName;
	}
	get _$AU() {
		return this._$AM._$AU;
	}
	constructor(e, t, n, r, i) {
		this.type = 1, this._$AH = W, this._$AN = void 0, this.element = e, this.name = t, this._$AM = r, this.options = i, n.length > 2 || n[0] !== "" || n[1] !== "" ? (this._$AH = Array(n.length - 1).fill(/* @__PURE__ */ new String()), this.strings = n) : this._$AH = W;
	}
	_$AI(e, t = this, n, r) {
		let i = this.strings, a = !1;
		if (i === void 0) e = Y(this, e, t, 0), a = !M(e) || e !== this._$AH && e !== U, a && (this._$AH = e);
		else {
			let r = e, o, s;
			for (e = i[0], o = 0; o < i.length - 1; o++) s = Y(this, r[n + o], t, o), s === U && (s = this._$AH[o]), a ||= !M(s) || s !== this._$AH[o], s === W ? e = W : e !== W && (e += (s ?? "") + i[o + 1]), this._$AH[o] = s;
		}
		a && !r && this.j(e);
	}
	j(e) {
		e === W ? this.element.removeAttribute(this.name) : this.element.setAttribute(this.name, e ?? "");
	}
}, he = class extends Z {
	constructor() {
		super(...arguments), this.type = 3;
	}
	j(e) {
		this.element[this.name] = e === W ? void 0 : e;
	}
}, ge = class extends Z {
	constructor() {
		super(...arguments), this.type = 4;
	}
	j(e) {
		this.element.toggleAttribute(this.name, !!e && e !== W);
	}
}, _e = class extends Z {
	constructor(e, t, n, r, i) {
		super(e, t, n, r, i), this.type = 5;
	}
	_$AI(e, t = this) {
		if ((e = Y(this, e, t, 0) ?? W) === U) return;
		let n = this._$AH, r = e === W && n !== W || e.capture !== n.capture || e.once !== n.once || e.passive !== n.passive, i = e !== W && (n === W || r);
		r && this.element.removeEventListener(this.name, this, n), i && this.element.addEventListener(this.name, this, e), this._$AH = e;
	}
	handleEvent(e) {
		typeof this._$AH == "function" ? this._$AH.call(this.options?.host ?? this.element, e) : this._$AH.handleEvent(e);
	}
}, ve = class {
	constructor(e, t, n) {
		this.element = e, this.type = 6, this._$AN = void 0, this._$AM = t, this.options = n;
	}
	get _$AU() {
		return this._$AM._$AU;
	}
	_$AI(e) {
		Y(this, e);
	}
}, ye = C.litHtmlPolyfillSupport;
ye?.(J, X), (C.litHtmlVersions ??= []).push("3.3.3");
var be = (e, t, n) => {
	let r = n?.renderBefore ?? t, i = r._$litPart$;
	if (i === void 0) {
		let e = n?.renderBefore ?? null;
		r._$litPart$ = i = new X(t.insertBefore(j(), e), e, void 0, n ?? {});
	}
	return i._$AI(e), i;
}, Q = globalThis, $ = class extends S {
	constructor() {
		super(...arguments), this.renderOptions = { host: this }, this._$Do = void 0;
	}
	createRenderRoot() {
		let e = super.createRenderRoot();
		return this.renderOptions.renderBefore ??= e.firstChild, e;
	}
	update(e) {
		let t = this.render();
		this.hasUpdated || (this.renderOptions.isConnected = this.isConnected), super.update(e), this._$Do = be(t, this.renderRoot, this.renderOptions);
	}
	connectedCallback() {
		super.connectedCallback(), this._$Do?.setConnected(!0);
	}
	disconnectedCallback() {
		super.disconnectedCallback(), this._$Do?.setConnected(!1);
	}
	render() {
		return U;
	}
};
$._$litElement$ = !0, $.finalized = !0, Q.litElementHydrateSupport?.({ LitElement: $ });
var xe = Q.litElementPolyfillSupport;
xe?.({ LitElement: $ }), (Q.litElementVersions ??= []).push("4.2.2");
// src/components/log-settings-page.ts
var Se = class extends $ {
	constructor(...e) {
		super(...e), this.baseUrl = "", this.settingsPath = "/api/log/settings", this.deviceName = "logger", this.maxHostLength = 63, this.formHost = "", this.formUri = "", this.formPort = "", this.formEnabled = !1, this.loading = !1, this.saving = !1, this.notice = null, this.loaded = !1, this._loadStarted = !1;
	}
	static {
		this.properties = {
			baseUrl: {
				type: String,
				attribute: "base-url"
			},
			settingsPath: {
				type: String,
				attribute: "settings-path"
			},
			deviceName: {
				type: String,
				attribute: "device-name"
			},
			maxHostLength: {
				type: Number,
				attribute: "max-host-length"
			},
			formHost: { state: !0 },
			formUri: { state: !0 },
			formPort: { state: !0 },
			formEnabled: { state: !0 },
			loading: { state: !0 },
			saving: { state: !0 },
			notice: { state: !0 },
			loaded: { state: !0 }
		};
	}
	static {
		this.styles = te`
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
	}
	connectedCallback() {
		super.connectedCallback(), this.load();
	}
	firstUpdated() {
		this._loadStarted || this.load();
	}
	render() {
		return H`
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
              @input=${(e) => this.handleHostInput(e)}
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
              @input=${(e) => this.handleUriInput(e)}
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
              @input=${(e) => this.handlePortInput(e)}
            />
          </label>
          <label class="checkbox-row" for="edu-log-enabled">
            <input
              id="edu-log-enabled"
              type="checkbox"
              .checked=${this.formEnabled}
              ?disabled=${this.loading || this.saving}
              @change=${(e) => this.handleEnabledChange(e)}
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
              ${this.saving ? "Saving…" : "Save"}
            </button>
            <button
              type="button"
              data-action="reload"
              ?disabled=${this.loading || this.saving}
              @click=${() => void this.load()}
            >
              ${this.loading ? "Loading…" : "Reload"}
            </button>
          </div>
        </section>
      </div>
    `;
	}
	renderNotice(e) {
		return e ? H`<div
      class="banner banner-${e.kind}"
      role=${e.kind === "error" ? "alert" : "status"}
    >
      ${e.text}
    </div>` : W;
	}
	createClient() {
		return l({
			baseUrl: this.baseUrl,
			settingsPath: this.settingsPath
		});
	}
	async load() {
		if (!(this.loading || this.saving)) {
			this._loadStarted = !0, this.loading = !0, this.notice = null;
			try {
				let e = await this.createClient().getSettings();
				if (!e.ok) {
					this.notice = {
						kind: "error",
						text: e.message
					};
					return;
				}
				this.applySettings(e.data), this.loaded = !0;
			} finally {
				this.loading = !1;
			}
		}
	}
	applySettings(e) {
		this.formHost = e.host, this.formUri = e.uri, this.formPort = String(e.port), this.formEnabled = e.enabled;
	}
	handleHostInput(e) {
		this.formHost = e.currentTarget.value;
	}
	handleUriInput(e) {
		this.formUri = e.currentTarget.value;
	}
	handlePortInput(e) {
		this.formPort = e.currentTarget.value;
	}
	handleEnabledChange(e) {
		this.formEnabled = e.currentTarget.checked;
	}
	validate() {
		let e = this.formHost.trim(), t = this.formUri.trim();
		if (e.length === 0) return "Host is required.";
		if (e.length > this.maxHostLength) return `Host must be ${this.maxHostLength} characters or fewer.`;
		if (t.length === 0) return "URI is required.";
		if (t.length > this.maxHostLength) return `URI must be ${this.maxHostLength} characters or fewer.`;
		let n = this.formPort.trim();
		if (!/^\d+$/.test(n)) return "Port must be a whole number between 1 and 65535.";
		let r = Number(n);
		return !Number.isInteger(r) || r < 1 || r > 65535 ? "Port must be a whole number between 1 and 65535." : {
			host: e,
			uri: t,
			port: r,
			enabled: this.formEnabled
		};
	}
	async handleSave() {
		if (this.loading || this.saving) return;
		let e = this.validate();
		if (typeof e == "string") this.notice = {
			kind: "error",
			text: e
		};
		else {
			this.saving = !0, this.notice = null;
			try {
				let t = await this.createClient().saveSettings(e);
				if (!t.ok) {
					this.notice = {
						kind: "error",
						text: t.message
					};
					return;
				}
				this.applySettings(t.data), this.loaded = !0, this.notice = {
					kind: "info",
					text: "Settings saved."
				}, this.dispatchEvent(new CustomEvent("edu-log-saved", {
					detail: t.data,
					bubbles: !0,
					composed: !0
				}));
			} finally {
				this.saving = !1;
			}
		}
	}
};
typeof customElements < "u" && !customElements.get("edu-log-settings-page") && customElements.define("edu-log-settings-page", Se);
export { Se as EduLogSettingsPage, l as createLogSettingsApiClient };
