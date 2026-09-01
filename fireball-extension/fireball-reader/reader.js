/**
 * Fireball Reader View Content Script
 * Cleans page layout, strips advertisements and provides text-to-speech audio reader.
 */

function extractCleanArticle() {
  const article = document.querySelector('article') || document.querySelector('main') || document.body;
  const clone = article.cloneNode(true);

  // Strip ads, scripts, and trackers
  const junk = clone.querySelectorAll('script, style, iframe, nav, footer, aside, .ad, .ads, .banner');
  junk.forEach(el => el.remove());

  return {
    title: document.title,
    content: clone.innerText.trim()
  };
}

function speakArticle() {
  const data = extractCleanArticle();
  if ('speechSynthesis' in window) {
    window.speechSynthesis.cancel();
    const utterance = new SpeechSynthesisUtterance(data.content.slice(0, 2000));
    utterance.rate = 1.0;
    utterance.pitch = 1.0;
    window.speechSynthesis.speak(utterance);
  }
}

// Expose on window for extension command invocation
window.fireballReader = {
  extract: extractCleanArticle,
  speak: speakArticle
};
