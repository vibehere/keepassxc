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

    // Sites that force platform-only (e.g. some login UIs).
    if (pk.authenticatorSelection && pk.authenticatorSelection.authenticatorAttachment === "platform") {
      next.authenticatorSelection = Object.assign({}, pk.authenticatorSelection, {
        authenticatorAttachment: "cross-platform",
      });
    }

    // GitHub/Chrome often tag existing creds as transports:["internal"] only.
    // That makes Chromium skip USB/UHID security keys → instant auth failure.
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
