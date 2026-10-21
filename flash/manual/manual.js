
// 中文：搜索正文但不隐藏章节；点击结果定位到真实操作。
// English: Search all instructions and navigate to tasks without removing chapters.
document.querySelector('#toc-toggle').addEventListener('click',e=>{const open=document.querySelector('.toc').classList.toggle('open');e.currentTarget.setAttribute('aria-expanded',String(open));});
const dialog=document.querySelector('#image-dialog'),body=document.querySelector('#image-dialog-body');let openedFrom;
function openFigure(figure){if(!figure)return;openedFrom=document.activeElement;body.replaceChildren(figure.querySelector('svg').cloneNode(true));const legend=figure.querySelector('.legend');if(legend)body.append(legend.cloneNode(true));document.querySelector('#image-dialog-title').textContent=figure.querySelector('figcaption').textContent;dialog.showModal();}
document.querySelectorAll('.screen-button').forEach(b=>b.addEventListener('click',()=>openFigure(b.closest('figure'))));
document.querySelectorAll('[data-figure]').forEach(b=>b.addEventListener('click',()=>openFigure(document.querySelector(`#figure-${b.dataset.figure}`))));
document.querySelector('#close-dialog').addEventListener('click',()=>dialog.close());dialog.addEventListener('close',()=>openedFrom?.focus());dialog.addEventListener('click',e=>{if(e.target===dialog)dialog.close();});
const tocLinks=[...document.querySelectorAll('.toc a[href^="#"]')];
const observer=new IntersectionObserver(entries=>{for(const e of entries){if(e.isIntersecting){tocLinks.forEach(a=>{const active=a.hash==='#'+e.target.id;a.classList.toggle('active',active);if(active)a.setAttribute('aria-current','location');else a.removeAttribute('aria-current');});}}},{rootMargin:'-95px 0px -65% 0px'});document.querySelectorAll('.chapter').forEach(c=>observer.observe(c));
