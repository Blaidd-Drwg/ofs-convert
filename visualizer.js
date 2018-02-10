let prevColors = new Map();
function deselect() {
    for(const [rect, color] of prevColors)
        rect.setAttribute('fill', color);
    prevColors.clear();
}
document.children[0].onclick = deselect;
for(const rect of document.getElementsByTagName('rect')) {
    const tag = rect.getAttribute('class');
    if(tag === 'tag0')
        continue;
    const group = document.getElementsByClassName(tag);
    rect.onclick = function(event) {
        deselect();
        for(const rect of group) {
            prevColors.set(rect, rect.getAttribute('fill'));
            rect.setAttribute('fill', 'black');
        }
        event.stopPropagation();
    };
}
