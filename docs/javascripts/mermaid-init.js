/*
  docs/javascripts/mermaid-init.js — Mermaid initialization for MkDocs Material.

  Why: Allows small, readable architecture diagrams in docs without adding Node tooling.
*/

/* global mermaid */

document.addEventListener("DOMContentLoaded", () => {
  if (typeof mermaid === "undefined") {
    return;
  }

  mermaid.initialize({ startOnLoad: true });
});

