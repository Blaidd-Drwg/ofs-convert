let prevColors = new Map();
for(const rect of document.getElementsByTagName('rect')) {
    const tag = rect.getAttribute('class');
    if(tag === 'tag0')
        continue;
    const group = document.getElementsByClassName(tag);
    rect.onclick = function() {
        for(const [rect, color] of prevColors)
            rect.setAttribute('fill', color);
        prevColors.clear();
        for(const rect of group) {
            prevColors.set(rect, rect.getAttribute('fill'));
            rect.setAttribute('fill', 'black');
        }
    };
}
