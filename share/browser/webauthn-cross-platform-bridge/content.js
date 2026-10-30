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
    const selection = Object.assign({}, pk.authenticatorSelection || {});
    if (selection.authenticatorAttachment === "platform") {
      selection.authenticatorAttachment = "cross-platform";
    }
    // Prefer resident keys so OS Passkeys / USB discoverable creds work.
    if (!selection.residentKey && !selection.requireResidentKey) {
      selection.residentKey = "preferred";
    }
    return Object.assign({}, options, {
      publicKey: Object.assign({}, pk, {authenticatorSelection: selection}),
    });
  };

  if (navigator.credentials && typeof navigator.credentials.create === "function") {
    const originalCreate = navigator.credentials.create.bind(navigator.credentials);
    navigator.credentials.create = (options) => originalCreate(forceCrossPlatform(options));
  }

  if (navigator.credentials && typeof navigator.credentials.get === "function") {
    const originalGet = navigator.credentials.get.bind(navigator.credentials);
    navigator.credentials.get = (options) => originalGet(forceCrossPlatform(options));
  }

  if (window.PublicKeyCredential) {
    if (typeof PublicKeyCredential.isUserVerifyingPlatformAuthenticatorAvailable === "function") {
      PublicKeyCredential.isUserVerifyingPlatformAuthenticatorAvailable = async () => true;
    }
    if (typeof PublicKeyCredential.getClientCapabilities === "function") {
      const originalCaps = PublicKeyCredential.getClientCapabilities.bind(PublicKeyCredential);
      PublicKeyCredential.getClientCapabilities = async () => {
        const caps = await originalCaps();
        return Object.assign({}, caps, {
          userVerifyingPlatformAuthenticator: true,
          passkeyPlatformAuthenticator: true,
        });
      };
    }
  }
})();
