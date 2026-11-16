// ==UserScript==
// @name         WebAuthn cross-platform bridge
// @namespace    keepassxc-os-passkeys
// @version      1.0.5
// @description  Relax platform-only WebAuthn filters so USB and OS Passkeys security keys can be used.
// @match        *://*/*
// @run-at       document-start
// @grant        none
// ==/UserScript==

(() => {
  if (window.__kpxcWebAuthnCrossPlatformBridge) {
    return;
  }
  window.__kpxcWebAuthnCrossPlatformBridge = true;

  const forceCrossPlatform = (options) => {
    if (!options || !options.publicKey) {
      return options;
    }
    const pk = options.publicKey;
    const next = Object.assign({}, pk);

    if (pk.authenticatorSelection && pk.authenticatorSelection.authenticatorAttachment === "platform") {
      next.authenticatorSelection = Object.assign({}, pk.authenticatorSelection, {
        authenticatorAttachment: "cross-platform",
      });
    }

    if (Array.isArray(pk.allowCredentials)) {
      next.allowCredentials = pk.allowCredentials.map((cred) => {
        if (!cred || typeof cred !== "object") {
          return cred;
        }
        const transports = Array.isArray(cred.transports) ? cred.transports.slice() : null;
        if (!transports || transports.length === 0 || transports.includes("usb")) {
          return cred;
        }
        return Object.assign({}, cred, {transports: transports.concat(["usb"])});
      });
    }

    return Object.assign({}, options, {publicKey: next});
  };

  if (navigator.credentials && typeof navigator.credentials.create === "function") {
    const originalCreate = navigator.credentials.create.bind(navigator.credentials);
    navigator.credentials.create = (options) => originalCreate(forceCrossPlatform(options));
  }
  if (navigator.credentials && typeof navigator.credentials.get === "function") {
    const originalGet = navigator.credentials.get.bind(navigator.credentials);
    navigator.credentials.get = (options) => originalGet(forceCrossPlatform(options));
  }
})();
