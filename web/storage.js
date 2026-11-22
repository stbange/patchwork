mergeInto(LibraryManager.library, {
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
  browser_download: function (source) {
    try {
      var blob = new Blob([UTF8ToString(source)], {type: 'text/plain;charset=utf-8'});
      var url = URL.createObjectURL(blob);
      var link = document.createElement('a');
      link.href = url;
      link.download = 'patchwork-cutting-sheet.txt';
      link.click();
      setTimeout(function () { URL.revokeObjectURL(url); }, 1000);
      return 1;
    } catch (_) { return 0; }
  }
});
