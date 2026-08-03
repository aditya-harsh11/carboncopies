(function () {
  function init() {
    // Remove any button from the previous page before re-initialising
    document.querySelectorAll('.copy-md-wrapper').forEach(function (el) {
      el.remove();
    });

    var sourceEl = document.getElementById('page-markdown-source');
    if (!sourceEl) return;

    var article = document.querySelector('article.md-content__inner');
    if (!article) return;

    // Build absolute base URL for the markdown file's directory.
    // page.file.src_path is relative to docs/ e.g. "Research/BrainGenix/About.md"
    // With use_directory_urls the page URL has an extra level, so we resolve
    // images against the source file directory, not the page URL.
    var srcPath = (sourceEl.getAttribute('data-src-path') || '').replace(/\\/g, '/');
    var srcDir  = srcPath.substring(0, srcPath.lastIndexOf('/') + 1); // "Research/BrainGenix/"
    var markdownBase = window.location.origin + '/' + srcDir;

    var btn = document.createElement('button');
    btn.className = 'copy-md-btn';
    btn.setAttribute('aria-label', 'Copy page source as Markdown');
    btn.setAttribute('title', '');
    btn.innerHTML =
      '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="12" height="12" fill="currentColor" aria-hidden="true">' +
        '<path d="M19 21H8V7h11m0-2H8a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h11a2 2 0 0 0 2-2V7a2 2 0 0 0-2-2m-3-4H4a2 2 0 0 0-2 2v14h2V3h12V1Z"/>' +
      '</svg>' +
      '<span class="copy-md-label"></span>';

    btn.addEventListener('click', function () {
      var text = resolveImages(sourceEl.value, markdownBase);
      var label = btn.querySelector('.copy-md-label');

      function onCopied() {
        btn.classList.add('copy-md-btn--copied');
        label.textContent = 'Copied as Markdown!';
        setTimeout(function () {
          btn.classList.remove('copy-md-btn--copied');
          label.textContent = '';
        }, 2000);
      }

      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(text).then(onCopied).catch(function () {
          fallbackCopy(text, onCopied);
        });
      } else {
        fallbackCopy(text, onCopied);
      }
    });

    var wrapper = document.createElement('div');
    wrapper.className = 'copy-md-wrapper';
    wrapper.appendChild(btn);

    var h1 = article.querySelector('h1');
    if (h1) {
      h1.insertAdjacentElement('afterend', wrapper);
    } else {
      article.insertBefore(wrapper, article.firstChild);
    }
  }

  // Rewrite relative image URLs to absolute so they work when pasted elsewhere.
  function resolveImages(markdown, base) {
    // Inline markdown images: ![alt](url) and ![alt](url "title")
    markdown = markdown.replace(
      /!\[([^\]]*)\]\(([^\s)"]+)([^)]*)\)/g,
      function (match, alt, url, rest) {
        return '![' + alt + '](' + toAbsolute(url, base) + rest + ')';
      }
    );
    // HTML <img ...> tags → convert to ![alt](url) markdown format
    markdown = markdown.replace(
      /<img([^>]*)>/gi,
      function (match, attrs) {
        var srcMatch = attrs.match(/\ssrc=(["'])([^"']+)\1/i);
        if (!srcMatch) return match;
        var altMatch = attrs.match(/\salt=(["'])([^"']*)\1/i);
        var alt = altMatch ? altMatch[2] : '';
        return '![' + alt + '](' + toAbsolute(srcMatch[2], base) + ')';
      }
    );
    return markdown;
  }

  function toAbsolute(url, base) {
    // Already absolute
    if (/^https?:\/\//i.test(url) || /^\/\//.test(url)) return url;
    // Root-relative
    if (url.charAt(0) === '/') return window.location.origin + url;
    // Relative — resolve against the markdown file's directory
    try { return new URL(url, base).href; } catch (e) { return url; }
  }

  function fallbackCopy(text, onCopied) {
    var ta = document.createElement('textarea');
    ta.value = text;
    ta.style.cssText = 'position:fixed;top:0;left:0;opacity:0;pointer-events:none';
    document.body.appendChild(ta);
    ta.focus();
    ta.select();
    try { document.execCommand('copy'); onCopied(); } catch (e) {}
    document.body.removeChild(ta);
  }

  // Initial page load
  document.addEventListener('DOMContentLoaded', function () {
    init();
    // Subscribe to Material's instant-navigation observable so the button
    // re-initialises on every subsequent page transition without a full reload.
    if (typeof document$ !== 'undefined') {
      document$.subscribe(init);
    }
  });
})();
