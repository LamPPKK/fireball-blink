/**
 * Netscape Bookmark HTML Exporter and Parser
 * Compatible with Chrome, Firefox, Safari, Brave, and Fireball Mini Android
 */

export function exportBookmarksToNetscapeHtml(bookmarks) {
  const now = Math.floor(Date.now() / 1000);
  let html = `<!DOCTYPE NETSCAPE-Bookmark-file-1>
<!-- This is an automatically generated file.
     It will be read and overwritten.
     DO NOT EDIT! -->
<META HTTP-EQUIV="Content-Type" CONTENT="text/html; charset=UTF-8">
<TITLE>Bookmarks</TITLE>
<H1>Bookmarks</H1>
<DL><p>
    <DT><H3 ADD_DATE="${now}" LAST_MODIFIED="${now}" PERSONAL_TOOLBAR_FOLDER="true">Fireball Bookmarks</H3>
    <DL><p>
`;

  for (const item of bookmarks) {
    const title = escapeHtml(item.title || item.url);
    const url = escapeHtml(item.url);
    const addDate = item.dateAdded ? Math.floor(item.dateAdded / 1000) : now;
    html += `        <DT><A HREF="${url}" ADD_DATE="${addDate}">${title}</A>\n`;
  }

  html += `    </DL><p>
</DL><p>
`;
  return html;
}

export function parseNetscapeHtml(htmlContent) {
  const bookmarks = [];
  const regex = /<A\s+HREF=["']([^"']+)["'][^>]*>([^<]+)<\/A>/gi;
  let match;

  while ((match = regex.exec(htmlContent)) !== null) {
    const url = match[1].trim();
    const title = match[2].trim();
    if (url && (url.startsWith("http://") || url.startsWith("https://"))) {
      bookmarks.push({
        id: "bm-" + Math.random().toString(36).substring(2, 10),
        url: url,
        title: title,
        createdAtTimestamp: Date.now()
      });
    }
  }

  return bookmarks;
}

function escapeHtml(str) {
  return str
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#039;");
}
