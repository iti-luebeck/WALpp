// Interactive playground for the WAL++ documentation (docs/playground.md).
//
// Two independent WebAssembly modules share the page:
//
//   * WAL++ itself, built by build-wasm.sh from the same C++20 sources as the `wal`
//     CLI, talking to JS through embind (see src/wasm/wal_wasm.cpp).
//   * Surfer (https://gitlab.com/surfer-project/surfer, EUPL-1.2), used unmodified and
//     driven over WCP, the Waveform Control Protocol its wasm build exposes to JS.
//
// They do not share memory. A trace is handed to WAL++ as text and to Surfer as a URL,
// and results travel from WAL++ to the viewer as WCP commands.
//
// Material's `navigation.instant` swaps pages without a reload, so this builds itself on
// every `document$` emission and tears itself down when navigating away.

(function () {
  'use strict';

  // Surfer's eframe canvas id is hardcoded in its main.rs; any other id panics on start.
  var SURFER_CANVAS_ID = 'the_canvas_id';
  // WCP version this page speaks. Surfer refuses a greeting with any other version.
  var WCP_VERSION = '0';

  var state = null;

  // ---------------------------------------------------------------- utilities

  function el(tag, cls, text) {
    var node = document.createElement(tag);
    if (cls) node.className = cls;
    if (text != null) node.textContent = text;
    return node;
  }

  function loadScript(url) {
    return new Promise(function (resolve, reject) {
      var tag = document.createElement('script');
      tag.src = url;
      tag.onload = resolve;
      tag.onerror = function () { reject(new Error('cannot load ' + url)); };
      document.head.appendChild(tag);
    });
  }

  // Splits WAL source into top level forms, so an example with several expressions
  // reports each result separately, the way typing them one by one would.
  function splitForms(source) {
    var forms = [];
    var depth = 0, start = -1, inString = false, inComment = false;
    for (var i = 0; i < source.length; i++) {
      var c = source[i];
      if (inComment) { if (c === '\n') inComment = false; continue; }
      if (inString) {
        if (c === '\\') i++;
        else if (c === '"') inString = false;
        continue;
      }
      if (c === ';') { inComment = true; continue; }
      if (c === '"') { if (start < 0) start = i; inString = true; continue; }
      if (c === '(') { if (depth === 0) start = i; depth++; continue; }
      if (c === ')') {
        depth--;
        if (depth === 0 && start >= 0) { forms.push(source.slice(start, i + 1)); start = -1; }
        continue;
      }
      if (depth === 0 && start < 0 && !/\s/.test(c)) start = i;
      if (depth === 0 && start >= 0 && /\s/.test(c)) { forms.push(source.slice(start, i)); start = -1; }
    }
    if (start >= 0 && depth === 0) forms.push(source.slice(start));
    return forms.filter(function (f) { return f.trim().length; });
  }

  // Recognises a WAL result that is a list of (time value) pairs, which is what the
  // hamming operators and anything filtered out of them produce. Returns null otherwise.
  function parsePairs(printed) {
    if (!printed || printed[0] !== '(') return null;
    var pairs = [];
    var re = /\((-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)\)/g;
    var m;
    while ((m = re.exec(printed)) !== null) {
      pairs.push([Number(m[1]), Number(m[2])]);
    }
    if (!pairs.length) return null;
    // Guard against matching something that merely contains numbers in brackets.
    var bracketed = (printed.match(/\(/g) || []).length;
    if (pairs.length + 1 < bracketed) return null;
    return pairs;
  }

  // ---------------------------------------------------------------- the page

  function build(host) {
    var base = host.getAttribute('data-base') || './';
    var ui = {};
    var root = el('div', 'wal-pg');

    // toolbar
    var bar = el('div', 'wal-pg__bar');
    bar.appendChild(el('label', null, 'Trace'));
    ui.trace = el('select');
    bar.appendChild(ui.trace);
    ui.run = el('button', 'wal-pg__primary', 'Run');
    ui.send = el('button', null, 'Send to viewer');
    ui.reset = el('button', null, 'Reset session');
    ui.send.disabled = true;
    bar.appendChild(ui.run);
    bar.appendChild(ui.send);
    bar.appendChild(ui.reset);
    bar.appendChild(el('span', 'wal-pg__spacer'));
    ui.status = el('span', 'wal-pg__status', 'starting WAL++ ...');
    bar.appendChild(ui.status);
    root.appendChild(bar);

    // viewer
    var viewer = el('div', 'wal-pg__viewer');
    ui.canvas = document.createElement('canvas');
    ui.canvas.id = SURFER_CANVAS_ID;
    viewer.appendChild(ui.canvas);
    ui.overlay = el('div', 'wal-pg__overlay');
    ui.start = el('button', null, 'Start the waveform viewer');
    ui.overlay.appendChild(ui.start);
    ui.overlay.appendChild(el('p', null,
      'Surfer is a 15 MB WebAssembly bundle, so it is not downloaded until you ask for it. ' +
      'The WAL++ console below works either way.'));
    viewer.appendChild(ui.overlay);
    root.appendChild(viewer);

    // examples beside the console
    var lower = el('div', 'wal-pg__lower');
    ui.examples = el('div', 'wal-pg__examples');
    lower.appendChild(ui.examples);

    var console_ = el('div', 'wal-pg__console');
    ui.editor = el('textarea', 'wal-pg__editor');
    ui.editor.spellcheck = false;
    ui.editor.setAttribute('aria-label', 'WAL expressions');
    ui.editor.value = '(+ 1 2 3)';
    console_.appendChild(ui.editor);
    ui.out = el('div', 'wal-pg__out');
    console_.appendChild(ui.out);
    lower.appendChild(console_);
    root.appendChild(lower);

    host.appendChild(root);
    return ui;
  }

  function say(ui, text, cls) {
    var line = el('span', cls, text + '\n');
    ui.out.appendChild(line);
    ui.out.scrollTop = ui.out.scrollHeight;
  }

  // ---------------------------------------------------------------- WAL++ side

  function evaluate(ctx, source) {
    var forms = splitForms(source);
    var last = null;
    forms.forEach(function (form) {
      say(ctx.ui, 'WAL> ' + form, 'is-prompt');
      var raw;
      try {
        raw = JSON.parse(ctx.session.eval(form));
      } catch (e) {
        say(ctx.ui, 'bridge error: ' + e, 'is-error');
        return;
      }
      if (raw.out) say(ctx.ui, raw.out.replace(/\n$/, ''));
      if (raw.err) { say(ctx.ui, 'error: ' + raw.err, 'is-error'); return; }
      if (raw.val) say(ctx.ui, raw.val);
      last = raw.val;
    });

    ctx.pairs = parsePairs(last);
    ctx.ui.send.disabled = !(ctx.pairs && ctx.wcpReady);
    if (ctx.pairs && !ctx.wcpReady) {
      say(ctx.ui, ctx.pairs.length + ' (time value) pairs. Start the viewer to mark them.', 'is-note');
    } else if (ctx.pairs) {
      say(ctx.ui, ctx.pairs.length + ' (time value) pairs. Send to viewer marks them.', 'is-note');
    }
  }

  function loadTrace(ctx, name) {
    if (!name) { ctx.trace = null; return Promise.resolve(); }
    var url = ctx.base + 'traces/' + name;
    return fetch(url).then(function (r) {
      if (!r.ok) throw new Error(r.status + ' for ' + url);
      return r.text();
    }).then(function (text) {
      ctx.session.reset();
      var err = ctx.session.loadTrace(name, text, 'DEFAULT');
      if (err) { say(ctx.ui, 'could not load ' + name + ': ' + err, 'is-error'); return; }
      ctx.trace = name;
      say(ctx.ui, 'loaded ' + name + ' into WAL++', 'is-ok');
      if (ctx.wcpReady) return viewerLoad(ctx, url);
    }).catch(function (e) {
      say(ctx.ui, 'could not fetch ' + url + ': ' + e.message, 'is-error');
    });
  }

  // ---------------------------------------------------------------- viewer side

  function wcp(ctx, command) {
    return ctx.surfer.handle_wcp_cs_message(JSON.stringify(
      Object.assign({ type: 'command' }, command)));
  }

  // Surfer answers on its own channel; this drains it for as long as the page lives.
  function pumpWcp(ctx) {
    if (!ctx.alive) return;
    ctx.surfer.next_wcp_sc_message().then(function (message) {
      if (!ctx.alive) return;
      if (message == null) { setTimeout(function () { pumpWcp(ctx); }, 60); return; }
      var parsed;
      try { parsed = JSON.parse(message); } catch (e) { parsed = null; }
      if (parsed && parsed.type === 'error') {
        say(ctx.ui, 'viewer: ' + (parsed.message || parsed.error), 'is-error');
      } else if (parsed && parsed.type === 'event' && parsed.event === 'waveforms_loaded') {
        wcp(ctx, { command: 'zoom_to_fit', viewport_idx: 0 });
      }
      pumpWcp(ctx);
    }).catch(function () {
      if (ctx.alive) setTimeout(function () { pumpWcp(ctx); }, 250);
    });
  }

  function viewerLoad(ctx, url) {
    var absolute = new URL(url, window.location.href).href;
    return wcp(ctx, { command: 'clear' })
      .then(function () { return wcp(ctx, { command: 'load', source: absolute }); })
      .then(function () {
        // add_scope needs the waveform in place; waveforms_loaded arrives asynchronously.
        setTimeout(function () {
          var scope = ctx.trace && ctx.trace.indexOf('counter') === 0 ? 'cpu' : 'dut';
          wcp(ctx, { command: 'add_scope', scope: scope, recursive: true });
          wcp(ctx, { command: 'zoom_to_fit', viewport_idx: 0 });
        }, 700);
      });
  }

  function startViewer(ctx) {
    ctx.ui.start.disabled = true;
    ctx.ui.status.textContent = 'downloading the viewer (15 MB) ...';
    say(ctx.ui, 'downloading Surfer ...', 'is-note');

    import(ctx.base + 'surfer/surfer.js').then(function (mod) {
      return mod.default({ module_or_path: ctx.base + 'surfer/surfer_bg.wasm' })
        .then(function () { return mod; });
    }).then(function (mod) {
      ctx.surfer = mod;
      ctx.ui.overlay.style.display = 'none';
      new mod.WebHandle().start(ctx.ui.canvas);
      ctx.ui.status.textContent = 'viewer running';
      return new Promise(function (r) { setTimeout(r, 600); });
    }).then(function () {
      return ctx.surfer.start_wcp();
    }).then(function () {
      pumpWcp(ctx);
      return ctx.surfer.handle_wcp_cs_message(JSON.stringify(
        { type: 'greeting', version: WCP_VERSION, commands: [] }));
    }).then(function () {
      ctx.wcpReady = true;
      say(ctx.ui, 'viewer ready, speaking WCP v' + WCP_VERSION, 'is-ok');
      ctx.ui.send.disabled = !ctx.pairs;
      if (ctx.trace) return viewerLoad(ctx, ctx.base + 'traces/' + ctx.trace);
    }).catch(function (e) {
      ctx.ui.start.disabled = false;
      ctx.ui.overlay.style.display = '';
      ctx.ui.status.textContent = 'viewer unavailable';
      say(ctx.ui, 'the viewer could not start: ' + e.message, 'is-error');
      say(ctx.ui, 'Run tools/fetch-surfer.sh and rebuild the docs to bundle it.', 'is-note');
    });
  }

  function sendMarkers(ctx) {
    if (!ctx.pairs || !ctx.wcpReady) return;
    var pairs = ctx.pairs.slice(0, 64);
    var markers = pairs.map(function (p, i) {
      return { time: Math.round(p[0]), name: String(p[1]), move_focus: i === 0 };
    });
    wcp(ctx, { command: 'add_markers', markers: markers }).then(function () {
      var times = pairs.map(function (p) { return p[0]; });
      return wcp(ctx, {
        command: 'set_viewport_range',
        start: Math.round(Math.min.apply(null, times)),
        end: Math.round(Math.max.apply(null, times)) || 1
      });
    });
    say(ctx.ui, 'marked ' + markers.length + ' timestamps in the viewer'
      + (ctx.pairs.length > pairs.length ? ' (first 64 of ' + ctx.pairs.length + ')' : ''), 'is-ok');
  }

  // ---------------------------------------------------------------- examples

  function renderExamples(ctx, catalogue) {
    ctx.ui.examples.innerHTML = '';
    var buttons = [];

    Object.keys(catalogue.traces || {}).forEach(function (name) {
      var option = el('option', null, catalogue.traces[name].title + ' (' + name + ')');
      option.value = name;
      ctx.ui.trace.appendChild(option);
    });
    var none = el('option', null, 'no trace');
    none.value = '';
    ctx.ui.trace.insertBefore(none, ctx.ui.trace.firstChild);
    ctx.ui.trace.value = ctx.trace || '';

    catalogue.groups.forEach(function (group) {
      ctx.ui.examples.appendChild(el('h3', null, group.title));
      group.examples.forEach(function (example) {
        var button = el('button');
        button.appendChild(document.createTextNode(example.title));
        button.appendChild(el('span', 'wal-pg__blurb', example.blurb));
        button.addEventListener('click', function () {
          buttons.forEach(function (b) { b.classList.remove('is-active'); });
          button.classList.add('is-active');
          ctx.ui.editor.value = example.code.join('\n');
          ctx.ui.trace.value = example.trace || '';
          var done = example.trace && example.trace !== ctx.trace
            ? loadTrace(ctx, example.trace)
            : Promise.resolve();
          done.then(function () { evaluate(ctx, ctx.ui.editor.value); });
        });
        buttons.push(button);
        ctx.ui.examples.appendChild(button);
      });
    });
  }

  // ---------------------------------------------------------------- lifecycle

  function teardown() {
    document.body.classList.remove('walpp-playground');
    if (state) { state.alive = false; state = null; }
  }

  function init() {
    var host = document.getElementById('wal-playground');
    if (!host) { teardown(); return; }
    if (state && state.host === host) return;
    teardown();
    document.body.classList.add('walpp-playground');

    var ui = build(host);
    // Resolve the asset base against the PAGE, not this script: a dynamic import() in a
    // classic script resolves relative to the script's own URL, which would look for the
    // viewer next to playground.js instead of next to the page.
    var base = new URL(host.getAttribute('data-base') || './', window.location.href).href;
    var ctx = state = {
      host: host,
      ui: ui,
      base: base,
      alive: true,
      wcpReady: false,
      pairs: null,
      trace: null
    };

    ui.run.addEventListener('click', function () { evaluate(ctx, ui.editor.value); });
    ui.send.addEventListener('click', function () { sendMarkers(ctx); });
    ui.start.addEventListener('click', function () { startViewer(ctx); });
    ui.reset.addEventListener('click', function () {
      ctx.session.reset();
      ui.out.innerHTML = '';
      ctx.pairs = null;
      ui.send.disabled = true;
      say(ui, 'session reset', 'is-note');
      if (ui.trace.value) loadTrace(ctx, ui.trace.value);
    });
    ui.trace.addEventListener('change', function () { loadTrace(ctx, ui.trace.value); });
    // Ctrl/Cmd+Enter runs, like a notebook cell.
    ui.editor.addEventListener('keydown', function (e) {
      if ((e.ctrlKey || e.metaKey) && e.key === 'Enter') { e.preventDefault(); evaluate(ctx, ui.editor.value); }
    });

    loadScript(ctx.base + 'walpp.js').then(function () {
      return window.createWalpp();
    }).then(function (module) {
      if (!ctx.alive) return;
      ctx.module = module;
      ctx.session = new module.Session();
      ui.status.textContent = 'WAL++ ready';
      say(ui, 'WAL++ ready. Pick an example, or type an expression and press Run.', 'is-ok');
      return fetch(ctx.base + 'examples.json').then(function (r) { return r.json(); });
    }).then(function (catalogue) {
      if (!ctx.alive || !catalogue) return;
      renderExamples(ctx, catalogue);
    }).catch(function (e) {
      ui.status.textContent = 'WAL++ unavailable';
      say(ui, 'WAL++ could not start: ' + e.message, 'is-error');
      say(ui, 'Run ./build-wasm.sh and rebuild the docs to bundle it.', 'is-note');
    });
  }

  document.addEventListener('DOMContentLoaded', init);
  if (typeof document$ !== 'undefined' && document$.subscribe) {
    document$.subscribe(function () { setTimeout(init, 50); });
  }
})();
