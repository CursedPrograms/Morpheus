// Morpheus page: feature cards, QR code and the year.
(function () {
  'use strict';
  const features = [
    ['Seven panels', 'Project, Composition, Timeline, Effect Controls, Tools, Preview and Info, laid out in the Atelier studio look: dark greys and the Atelier blue.'],
    ['The project model', 'Compositions with size, frame rate and duration. Layers: solid, shape, text, image, footage, nested composition, null, camera and light.'],
    ['Transforms', 'Every layer has an anchor point, position, scale, rotation and opacity. In and out points trim it in the Timeline.'],
    ['Save and open', 'Projects are saved as .morpheus JSON files. Open them from the File menu or drop them on the window.'],
    ['Twelve languages', 'English, Japanese, Chinese, Spanish, Korean, French, German, Russian, Serbian, Afrikaans, Yucatec Maya and Pennsylvania Dutch.'],
    ['Coming: rendering, keyframes, effects', 'Phase 3 renders a composition to screen. Phase 5 adds keyframes and the graph editor. Phase 7 adds the effects stack.']
  ];
  const grid = document.getElementById('features');
  if (grid) {
    for (const [title, text] of features) {
      const card = document.createElement('article');
      const h = document.createElement('h2');
      h.textContent = title;
      const p = document.createElement('p');
      p.textContent = text;
      card.append(h, p);
      grid.appendChild(card);
    }
  }
  // QR code pointing at this page (via Google Charts API).
  const qrImg = document.getElementById('qr-img');
  if (qrImg) {
    const url = encodeURIComponent('https://cursedprograms.github.io/Morpheus/');
    qrImg.src = 'https://chart.googleapis.com/chart?cht=qr&chs=200x200&chl=' + url;
  }
  const year = document.getElementById('year');
  if (year) year.textContent = String(Math.max(2026, new Date().getFullYear()));
})();
