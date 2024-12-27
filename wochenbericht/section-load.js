function get(theUrl) {
    var xmlHttp = new XMLHttpRequest();
    xmlHttp.open("GET", theUrl, true);
    xmlHttp.send(null);
    return xmlHttp.responseText;
}

const result = document.createElement("div");
result.id = "page-content";

const section_amount = Number(get("/api/get/section-amount"));

for (let i = 0; i < section_amount; i++) {
    const section = document.createElement("div");
    section.className = "section";
    const script = document.createElement("script");
    script.text = get("/section-load.js");
    section.appendChild(script);
    result.appendChild(section);
}

document.currentScript.replaceWith(result);