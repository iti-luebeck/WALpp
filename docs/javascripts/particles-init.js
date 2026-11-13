// Animated particle hero for the WAL++ documentation front page, after the one in
// BuddelChip and polycore, in the blues of the WAL++ logo.
//
// Builds a parallax hero (a grid of synchronisation lines + a tsParticles canvas drawing
// signals across it) and injects it
// above the README content, but ONLY on the site root. Everything is created from
// JS so no theme template override is needed; the matching styles live in
// docs/stylesheets/hero.css.
//
// Material's `navigation.instant` swaps pages without a reload, so this runs both
// on DOMContentLoaded and on every `document$` emission, and tears itself down
// again when navigating away from the front page.

(function () {
  'use strict';

  // The logo's blues (see docs/stylesheets/walpp.css): cyan at the top of the mark,
  // blue at the bottom, and the mid tone between them.
  var WALPP_CYAN = '#08dbf4';
  var WALPP_MID = '#0794e9';
  var WALPP_BLUE = '#037ee6';

  // Path generator for tsParticles: a particle draws a digital signal from left to right,
  // like a trace in a waveform viewer - level stretches and edges, and every edge starts
  // on one of the synchronisation lines GRID pixels apart that hero.css draws behind it.
  // Clocks have vertical edges; a data signal has either vertical or 45 degree ones.
  // The generator moves the particle itself and hands the engine a zero velocity: the
  // engine scales any velocity by its own speed factor, so edges would end a little
  // above or below the 0 and 1 levels and the signals of a lane would not line up.
  var WAVEFORM_PATH = 'walppWaveformPath';
  var GRID = 80;  // CSS px between synchronisation lines; keep in step with hero.css
  var LANE = 56;  // CSS px from one signal to the next, as rows in a waveform viewer
  var SWING = 22; // CSS px between a signal's low and high level; less than LANE
  var REDRAW_S = 5; // seconds between two signals on the same lane
  var SPEED = 1.5;  // CSS px a signal advances per frame at 60 fps; the trail draws one
                    // dot per frame, so a larger step than the dot leaves a dotted line
  var DIAG = Math.SQRT1_2;
  var MARKER_FADE_MS = 4000;  // a click marker stays this long; keep in step with hero.css

  function waveformPath() {
    var ratio = 1;  // canvas pixels per CSS pixel (detectRetina)

    // Lane i has its 0 level at (i + 0.5) * LANE + SWING / 2, where loadParticles puts
    // its emitter. A particle takes the lane nearest to where it starts: the engine moves
    // it once with a random velocity before the generator runs, so its own starting
    // height is a few pixels off. The lane also fixes the kind of signal (clock, data
    // with 45 degree edges, data with vertical edges), so all particles of one lane draw
    // the same kind of signal on the same two levels.
    function laneOf(y) {
      return Math.max(0, Math.round((y / ratio - SWING / 2) / LANE - 0.5));
    }

    function laneKind(lane) {
      var h = (lane * 2654435761) >>> 0;
      return h % 4;  // 0 clock, 1 data with 45 degree edges, 2-3 data with vertical edges
    }

    // Moves the particle one step along an edge, straight or 45 degrees forward, and ends
    // exactly on the level.
    function edgeStep(p, step) {
      var target = p.wfHigh ? p.wfTop : p.wfLow;
      var left = target - p.position.y;
      var dy = Math.max(-step, Math.min(step, left));
      if (Math.abs(left) <= step) p.wfEdge = false;
      p.position.y += dy;
      if (p.wfSlant) p.position.x += Math.abs(dy);
    }

    return {
      init: function (container) {
        ratio = (container.retina && container.retina.pixelRatio) || 1;
      },
      update: function () {},
      reset: function (p) {
        delete p.wfInit; delete p.wfLine; delete p.wfEdge; delete p.wfHigh;
        delete p.wfLow; delete p.wfTop; delete p.wfClock; delete p.wfSlant;
      },
      generate: function (p, delta) {
        var grid = GRID * ratio;
        var step = SPEED * ratio * ((delta && delta.factor) || 1);
        var still = { x: 0, y: 0, z: 0 };  // the engine adds z too
        p.velocity.x = 0;
        p.velocity.y = 0;

        if (p.wfInit === undefined) {
          var lane = laneOf(p.position.y);
          var kind = laneKind(lane);
          p.wfInit = true;
          p.position.x = Math.max(0, p.position.x);
          p.wfLine = Math.floor(p.position.x / grid);
          p.wfEdge = false;
          p.wfLow = ((lane + 0.5) * LANE + SWING / 2) * ratio;
          p.wfTop = p.wfLow - SWING * ratio;
          p.wfClock = kind === 0;              // a clock toggles on every line
          p.wfSlant = kind === 1;
          p.wfHigh = Math.random() < 0.5;
          p.position.y = p.wfHigh ? p.wfTop : p.wfLow;
        }

        if (p.wfEdge) {
          edgeStep(p, step);
          return still;
        }

        p.position.x += step;
        var line = Math.floor(p.position.x / grid);
        if (line !== p.wfLine) {
          p.wfLine = line;
          if (p.wfClock || Math.random() < 0.4) {
            p.position.x = line * grid;
            p.wfHigh = !p.wfHigh;
            p.wfEdge = true;
          }
        }
        return still;
      }
    };
  }

  // Live tsParticles instance, so teardown can stop its animation loop instead of
  // orphaning it when the canvas DOM is removed (matters because a light/dark
  // toggle tears down and rebuilds the whole hero).
  var particleContainer = null;

  // The site is served below /docs/walpp/, so the path alone says little. Material
  // writes the way back to the site root into #__config; on the root page it is ".".
  function isFrontPage() {
    var el = document.getElementById('__config');
    if (!el) return false;
    try { return JSON.parse(el.textContent).base === '.'; } catch (e) { return false; }
  }

  function teardown() {
    document.body.classList.remove('bc-homepage');
    if (particleContainer) {
      try { particleContainer.destroy(); } catch (e) { /* already gone */ }
      particleContainer = null;
    }
    var hero = document.querySelector('.bc-parallax');
    var section = document.querySelector('.bc-hero');
    if (hero) hero.remove();
    if (section) section.remove();
    var inner = document.querySelector('.md-content__inner');
    if (inner) inner.classList.remove('bc-has-hero');
  }

  function buildHero() {
    var main = document.querySelector('.md-main');
    var inner = document.querySelector('.md-content__inner');
    if (!main || !inner) return null;

    var parallax = document.createElement('div');
    parallax.className = 'bc-parallax';
    main.insertBefore(parallax, main.firstChild);

    var hero = document.createElement('div');
    hero.className = 'bc-hero';
    hero.innerHTML = [
      '<img class="bc-hero__logo" src="docs/images/walpp-logo.png" alt="WAL++ logo">',
      '<h1 class="bc-hero__title">WAL++</h1>',
      '<p class="bc-hero__tagline">Waveform Analysis Language in C++</p>',
      '<p class="bc-hero__description">',
      'Analyse VCD, FST and CSV traces with the S-expression language of WAL &mdash; ',
      'the same 130+ operators, rewritten in C++20 for large waveforms, with a REPL, ',
      'a command line and Python bindings.',
      '</p>',
      '<div class="bc-hero__buttons">',
      '  <a href="docs/API_QUICK_REFERENCE/" class="bc-hero__button bc-hero__button--primary">Quick start</a>',
      '  <a href="api/" class="bc-hero__button bc-hero__button--secondary">API reference</a>',
      '</div>'
    ].join('');
    inner.insertBefore(hero, inner.firstChild);
    inner.classList.add('bc-has-hero');

    // Page background drives the particle trail, so the effect stays coherent when the
    // reader toggles light/dark.
    var bodyBg = window.getComputedStyle(document.body).backgroundColor || '#ffffff';

    // Layer 1 - the synchronisation lines the edges snap to (drawn in hero.css).
    var grid = document.createElement('div');
    grid.className = 'bc-parallax__layer bc-parallax__layer--grid';
    grid.setAttribute('data-speed', '0.5');
    parallax.appendChild(grid);

    // Layer 2 - the particle canvas itself. Same speed as the grid, so the edges stay
    // on their lines while the page scrolls.
    var particles = document.createElement('div');
    particles.className = 'bc-parallax__layer bc-parallax__layer--particles';
    particles.id = 'bc-particles';
    particles.setAttribute('data-speed', '0.5');
    parallax.appendChild(particles);

    return { bodyBg: bodyBg };
  }

  // A click on the front page drops a cursor like in a waveform viewer: an orange
  // vertical line that grows from the click point up and down, then fades. It lives in
  // the particle layer, so it scrolls with the signals. The content sits above that
  // layer, so the listener is on the document (registered once, like the scheme
  // watcher) and leaves links, buttons and the header alone.
  function onMarkerClick(ev) {
    if (!document.body.classList.contains('bc-homepage')) return;
    if (ev.target.closest && ev.target.closest('a, button, input, label, .md-header, .md-search')) return;
    var layer = document.getElementById('bc-particles');
    if (!layer) return;
    var box = layer.getBoundingClientRect();
    if (ev.clientY < box.top || ev.clientY > box.bottom) return;
    var marker = document.createElement('div');
    marker.className = 'bc-marker';
    marker.style.left = (ev.clientX - box.left) + 'px';
    marker.style.transformOrigin = '50% ' + (ev.clientY - box.top) + 'px';
    layer.appendChild(marker);
    setTimeout(function () { marker.remove(); }, MARKER_FADE_MS);
  }

  function attachParallax() {
    var ticking = false;
    function update() {
      var scrolled = window.pageYOffset;
      document.querySelectorAll('.bc-parallax__layer').forEach(function (layer) {
        var speed = parseFloat(layer.getAttribute('data-speed')) || 0.5;
        layer.style.transform = 'translate3d(0, ' + (-(scrolled * speed)) + 'px, 0)';
      });
      ticking = false;
    }
    window.addEventListener('scroll', function () {
      if (!ticking) {
        window.requestAnimationFrame(update);
        ticking = true;
      }
    }, { passive: true });
    update();
  }

  function loadParticles(bodyBg) {
    if (typeof tsParticles === 'undefined') return;

    // Each particle draws a digital waveform (see waveformPath below).
    var ready = tsParticles.addPathGenerator
      ? Promise.resolve(tsParticles.addPathGenerator(WAVEFORM_PATH, waveformPath(), true))
      : Promise.resolve();

    // One emitter per lane on the left edge, LANE pixels apart over the whole stage, so
    // the signals sit one below the other. Each lane redraws its signal every REDRAW_S
    // seconds; the lanes start at random offsets within that, so they do not run in step.
    var stage = document.getElementById('bc-particles');
    var height = (stage && stage.offsetHeight) || 800;
    var lanes = Math.max(4, Math.floor(height / LANE));
    var emitters = [];
    for (var i = 0; i < lanes; i++) {
      emitters.push({
        autoPlay: true,
        fill: true,
        rate: { quantity: 1, delay: REDRAW_S },
        life: { delay: Math.random() * REDRAW_S },
        position: { x: 0, y: ((i + 0.5) * LANE + SWING / 2) * 100 / height },
        size: { mode: 'percent', height: 0, width: 0 }
      });
    }

    ready.then(function () {
      return tsParticles.load('bc-particles', {
        autoPlay: true,
        background: { color: { value: 'transparent' } },
        fpsLimit: 120,
        fullScreen: { enable: false, zIndex: 1 },
        // NB: fields below carry the sampled bodyBg (trail fillColor),
        // which is why a scheme change must rebuild rather than mutate in place.
        detectRetina: true,
        interactivity: {
          detectsOn: 'window',
          events: {
            onClick: { enable: false },  // a click drops a marker instead, see onMarkerClick
            onHover: { enable: true, mode: 'bubble' },
            resize: { enable: true }
          },
          modes: { bubble: { distance: 200, duration: 0.4, mix: false } }
        },
        particles: {
          color: {
            // The logo's three blues - each emitted particle (and its trace) takes one.
            value: [WALPP_CYAN, WALPP_MID, WALPP_BLUE]
          },
          move: {
            enable: true,
            direction: 'none',
            outModes: { default: 'destroy' },
            random: false,
            speed: 3,
            straight: false,
            trail: { enable: true, length: 100, fillColor: bodyBg },
            path: {
              clamp: false,
              delay: { value: 0 },
              enable: true,
              generator: WAVEFORM_PATH
            }
          },
          number: { value: 0, density: { enable: true, area: 800 } },
          opacity: { value: 1 },
          shape: { type: 'circle' },
          size: { value: 1.5 },  // dot radius: just over SPEED / 2, so the trail stays solid
          links: { enable: false }
        },
        emitters: emitters,
        smooth: false
      });
    }).then(function (container) {
      particleContainer = container;
    });
  }

  function init() {
    if (!isFrontPage()) {
      teardown();
      return;
    }
    document.body.classList.add('bc-homepage');
    if (document.querySelector('.bc-parallax')) return;  // already built

    var built = buildHero();
    if (!built) return;
    attachParallax();
    loadParticles(built.bodyBg);
  }

  // Rebuild from scratch: the trail fill bakes in the page
  // background at build time, so a light/dark switch needs a fresh sample.
  function rebuild() {
    if (!isFrontPage()) return;
    teardown();
    init();
  }

  // Watch Material's palette toggle. It flips data-md-color-scheme (default<->slate)
  // on <body>; the background then transitions over a few hundred ms, so settle
  // before re-sampling or the new particles inherit a mid-transition grey.
  var schemeTimer = null;
  function watchColorScheme() {
    if (typeof MutationObserver === 'undefined') return;
    var last = document.body.getAttribute('data-md-color-scheme');
    new MutationObserver(function () {
      var now = document.body.getAttribute('data-md-color-scheme');
      if (now === last) return;
      last = now;
      if (schemeTimer) clearTimeout(schemeTimer);
      schemeTimer = setTimeout(rebuild, 450);
    }).observe(document.body, {
      attributes: true,
      attributeFilter: ['data-md-color-scheme']
    });
  }

  document.addEventListener('DOMContentLoaded', function () {
    setTimeout(init, 300);
    watchColorScheme();
    document.addEventListener('click', onMarkerClick);
  });

  // Material instant-navigation hook.
  if (typeof document$ !== 'undefined' && document$.subscribe) {
    document$.subscribe(function () {
      setTimeout(init, 300);
    });
  }
})();
