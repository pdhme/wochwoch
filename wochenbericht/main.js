const updateIntervalTime = 2000;
let updateInterval;

async function get(url) {
	let responce = await fetch(url,{
		credentials: 'omit',
		headers: {
			"X-UID": localStorage.uid || ""
		}
	});
	localStorage.uid = responce.headers.get("X-UID");
	return await responce.text();
}

async function post(url, body) {
	let responce = await fetch(url,{
		method: "POST",
		credentials: 'omit',
		headers: {
			"X-UID": localStorage.uid || ""
		},
		body
	});
	localStorage.uid = responce.headers.get("X-UID");
	return responce;
}


async function answer(click) {
	if (click.target.nodeName != "SPAN" || click.target.parentElement.className != "q") {
		return;
	}
	const sid = [...document.getElementById("page-content").children].indexOf(click.target.parentElement.parentElement.parentElement) + 1;
	const qid = [...click.target.parentElement.parentElement.children].indexOf(click.target.parentElement) + 1;
	let responce = await post("/api/answer", sid + '\n' + qid + '\n' + click.target.textContent);
	if (!responce.ok) {
		responce.text().then(t=>alert(t));
	} else {
		manualUpdate();
	}
}

async function update() {
	const sections = [...document.getElementById("page-content").querySelectorAll(".section")];
	for (let i = 0; i < sections.length; i++) {
		let section = sections[i];
		const section_data_str = await get("/api/get/section/" + (i+1));
		const qData = JSON.parse(section_data_str);
		const qDOM = section.querySelectorAll(".q");
		for (let q = 0; q < qData.length; q++) {
			if (q >= qDOM.length) {
				break;
			}
			const aData = qData[q];
			const aDOM = [...qDOM[q].querySelectorAll("span")];
			if (aData == "TF") {
				// TODO
			} else {
				for (let a = 0; a < aDOM.length; a++) {
					if (!aDOM[a].textContent) {
						break;
					}

					if (aDOM[a].textContent == aData.correct) {
						aDOM[a].style.setProperty("--answer-highlight", "#00ff00ff")
					} else {
						aDOM[a].style.setProperty("--answer-highlight", "#00000000")
					}

					if (aDOM[a].textContent == aData.answer) {
						aDOM[a].style.borderColor = "#000000ff";
						if (aData.answer != aData.correct && aData.correct) {
							aDOM[a].style.setProperty("--answer-highlight", "#ff0000ff")
						}
					} else {
						aDOM[a].style.borderColor = "#00000000";
					}

				}
			}
		}
	}
}

function manualUpdate() {
	clearInterval(updateInterval);
	update();
	updateInterval = setInterval(update, updateIntervalTime);
}

function clear() {
	const sections = [...document.getElementById("page-content").querySelectorAll(".section")];
	for (let i = 0; i < sections.length; i++) {
		let section = sections[i];
		const questions = section.querySelectorAll(".q");
		for (let q = 0; q < questions.length; q++) {
			[...questions[q].querySelectorAll("span")].forEach((child)=>child.remove());
		}
	}
}


async function load() {
	clearInterval(updateInterval);
	const sections = [...document.getElementById("page-content").querySelectorAll(".section")];
	for (let i = 0; i < sections.length; i++) {
		let section = sections[i];
		const section_data_str = await get("/api/get/section/" + (i+1));
		const qData = JSON.parse(section_data_str);
		const qDOM = section.querySelectorAll(".q");
		for (let q = 0; q < qData.length; q++) {
			if (q >= qDOM.length) {
				break;
			}
			if (qData[q].answers == "TF") {
				// TODO
			} else {
				for (let a = 0; a < qData[q].answers.length; a++) {
					let answerElement = document.createElement("span");
					answerElement.textContent = qData[q].answers[a];
					qDOM[q].appendChild(answerElement);
					answerElement.addEventListener("click", answer);
				}
			}
		}
	}
	update();
	updateInterval = setInterval(update, updateIntervalTime);
}

document.addEventListener("DOMContentLoaded", load);
