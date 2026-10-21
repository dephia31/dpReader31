/* SPDX-License-Identifier: Apache-2.0
 * 中文：官网与说明书共用提问框；只展示说明书条目，不请求外部问答服务。
 * English: Shared manual finder; display documented instructions, without an external chat service.
 */
(() => {
  'use strict';
  const script = document.currentScript;
  const indexURL = new URL(script.dataset.index, location.href);
  const manualURL = new URL(script.dataset.manual, location.href);
  const examples = ['最近看的书排前面', '英文怎么打大写', 'WiFi 密码输错了', '恢复阅读进度'];
  let pending, search;
  function load() {
    if (!pending) pending = fetch(indexURL).then(r => {
      if (!r.ok) throw new Error('manual unavailable');
      return r.json();
    }).then(data => {
      search = KiikoManualSearch.create(data);
      return search;
    }).catch(error => { pending = null; throw error; });
    return pending;
  }
  const el = (tag, cls, text) => {
    const node = document.createElement(tag);
    if (cls) node.className = cls;
    if (text) node.textContent = text;
    return node;
  };
  const launch = el('button', 'manual-launch', '问说明书');
  launch.type = 'button'; launch.setAttribute('aria-haspopup', 'dialog');
  const dialog = el('dialog', 'manual-helper');
  dialog.setAttribute('aria-labelledby', 'manual-helper-title');
  const head = el('div', 'manual-helper-head');
  const heading = el('h2', '', '问说明书'); heading.id = 'manual-helper-title';
  const close = el('button', 'manual-helper-close', '关闭 ×'); close.type = 'button';
  const intro = el('p', 'manual-helper-intro', '输入关键词或一句话，找到对应操作和界面图。');
  const suggestions = el('div', 'manual-suggestions');
  const output = el('div', 'manual-answers');
  const status = el('p', 'manual-helper-status'); status.setAttribute('role', 'status');
  const form = el('form', 'manual-question');
  const label = el('label', 'manual-visually-hidden', '想了解什么操作？'); label.htmlFor = 'manual-question';
  const input = el('input'); input.id = 'manual-question'; input.type = 'search'; input.maxLength = 180;
  input.placeholder = '比如：最近看的书怎么排前面？'; input.autocomplete = 'off';
  const submit = el('button', '', '查找'); submit.type = 'submit';
  form.append(label, input, submit); head.append(heading, close);
  dialog.append(head, intro, suggestions, output, status, form); document.body.append(launch, dialog);
  let sequence = 0;
  const linkFor = id => { const url = new URL(manualURL); url.hash = id; return url.href; };
  async function ask(question) {
    const q = question.trim(); if (!q) { input.focus(); return; }
    const current = ++sequence;
    status.textContent = '正在查找说明书…'; output.replaceChildren();
    const asked = el('p', 'manual-asked', q); output.append(asked);
    try {
      const find = await load(); if (current !== sequence) return;
      const hits = find(q, 5);
      status.textContent = hits.length ? `找到 ${hits.length} 项相关操作，点击可查看完整图解。` : '没有找到相关操作。换个说法，或打开完整目录查找。';
      for (let n = 0; n < hits.length; n++) {
        const hit = hits[n], card = el('article', 'manual-answer');
        card.append(el('small', '', hit.chapter), el('h3', '', hit.title), el('p', 'manual-answer-path', hit.path));
        if (n === 0) {
          const list = el('ol');
          for (const step of hit.steps.slice(0, 3)) list.append(el('li', '', step));
          card.append(list);
        }
        const link = el('a', 'manual-answer-link', n === 0 ? '查看完整步骤与图解 →' : '定位到说明书 →');
        link.href = linkFor(hit.id);
        link.addEventListener('click', () => dialog.close()); card.append(link); output.append(card);
      }
      if (!hits.length) {
        const link = el('a', 'manual-answer-link', '打开全部说明书 →'); link.href = manualURL; output.append(link);
      }
    } catch {
      if (current !== sequence) return;
      status.textContent = '说明书暂时未加载成功，请再试一次。';
      const link = el('a', 'manual-answer-link', '打开完整说明书 →'); link.href = manualURL; output.append(link);
    }
    output.scrollTop = 0;
  }
  for (const question of examples) {
    const button = el('button', '', question); button.type = 'button';
    button.addEventListener('click', () => { input.value = question; ask(question); }); suggestions.append(button);
  }
  const show = () => { if (!dialog.open) dialog.showModal(); input.focus(); };
  launch.addEventListener('click', show);
  document.querySelectorAll('[data-ask-manual]').forEach(b => b.addEventListener('click', show));
  close.addEventListener('click', () => dialog.close());
  dialog.addEventListener('click', e => { if (e.target === dialog) dialog.close(); });
  form.addEventListener('submit', e => { e.preventDefault(); ask(input.value); });
  // 中文：完整说明书的搜索框与提问框使用同一份索引，支持直接定位。
  // English: The manual search field shares the same index and ranking as this dialog.
  const field = document.querySelector('#manual-search');
  if (field) {
    const results = document.querySelector('#search-results'), message = document.querySelector('#search-status');
    let revision = 0, timer;
    async function update() {
      const request = ++revision, q = field.value.trim(); results.replaceChildren();
      if (!q) { results.hidden = true; message.textContent = '支持关键词、模糊搜索，或直接输入一个问题。'; return; }
      results.hidden = false; message.textContent = '正在查找…';
      try {
        const find = await load(); if (request !== revision) return;
        const hits = find(q, 12); message.textContent = hits.length ? `找到 ${hits.length} 项相关操作` : '换个说法试试，例如“排序”“大小写”“密码”“恢复”。';
        for (const hit of hits) {
          const a = el('a', 'search-result'); a.href = '#' + hit.id;
          a.append(el('b', '', hit.title), el('small', '', hit.path));
          a.addEventListener('click', () => { results.hidden = true; document.querySelector(`#${hit.id} h3`)?.focus({ preventScroll: true }); }); results.append(a);
        }
      } catch { if (request === revision) message.textContent = '检索暂时未加载成功，请再试一次。'; }
    }
    field.addEventListener('input', () => { ++revision; clearTimeout(timer); timer = setTimeout(update, 90); });
    field.addEventListener('focus', () => { if (field.value.trim()) update(); });
    document.querySelector('#clear-search').addEventListener('click', () => { clearTimeout(timer); field.value = ''; update(); field.focus(); });
  }
})();
