mergeInto(LibraryManager.library, {
  browser_describe_view: function (source, keys, lang) {
    var description = UTF8ToString(source);
    var status = document.getElementById('current-view');
    if (status.textContent !== description) status.textContent = description;
    var language = UTF8ToString(lang);
    Module['patchworkLanguage'](language);
    var label = UTF8ToString(keys);
    if (Module.canvas.getAttribute('aria-label') !== label) Module.canvas.setAttribute('aria-label', label);
  },
  browser_load_prefs: function (target) {
    stringToUTF8(Module['patchworkSettings'], target, 6);
  },
  browser_save_prefs: function (source) {
    try {
      localStorage.setItem('patchwork.settings', UTF8ToString(source));
      return 1;
    } catch (_) { return 0; }
  },
  browser_load: function (target, capacity) {
    try {
      var data = localStorage.getItem('patchwork.plan.v1');
      if (data === null) return 0;
      if (!data.length || data.length >= capacity || /[^\x00-\x7f]/.test(data)) return -1;
      stringToUTF8(data, target, capacity);
      return data.length;
    } catch (_) { return -1; }
  },
  browser_save: function (source) {
    try {
      localStorage.setItem('patchwork.plan.v1', UTF8ToString(source));
      return 1;
    } catch (_) { return 0; }
  },
  browser_download: function (source, length, filename, pdf) {
    try {
      var type = pdf ? 'application/pdf' : 'text/plain;charset=utf-8';
      var blob = new Blob([HEAPU8.slice(source, source + length)], {type: type});
      var url = URL.createObjectURL(blob);
      var link = document.createElement('a');
      link.href = url;
      link.download = UTF8ToString(filename);
      link.click();
      setTimeout(function () { URL.revokeObjectURL(url); }, 1000);
      return 1;
    } catch (_) { return 0; }
  }
});
