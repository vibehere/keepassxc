(() => {
  const script = document.createElement("script");
  script.src = (typeof browser !== "undefined" ? browser : chrome).runtime.getURL("page.js");
  script.async = false;
  (document.documentElement || document.head).appendChild(script);
  script.remove();
})();
