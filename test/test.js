/*** BEGIN STRUCTS ****/
class Any {
	constructor() {
	}
}
class None {
	constructor() {
	}
}
class Canvas {
	constructor() {
	}
}
class Image {
	constructor() {
	}
}
class Rect {
	constructor(x, y, w, h) {
		this.x=x;
		this.y=y;
		this.w=w;
		this.h=h;
	}
}
class Point {
	constructor(x, y) {
		this.x=x;
		this.y=y;
	}
}
class Color {
	constructor(a, r, g, b) {
		this.a=a;
		this.r=r;
		this.g=g;
		this.b=b;
	}
}

/*** BEGIN SYSTEM/CANVAS ****/
function System_println(msg){
	console.log(msg);
}
function System_input(msg){
	return window.prompt(msg);
}let canvas;let context;let mousePos = new Point(0, 0);canvas.addEventListener('mousemove', function(e) {var cRect = canvas.getBoundingClientRect();mousePos.x = Math.round(e.clientX - cRect.left); mousePos.y = Math.round(e.clientY - cRect.top);});function Canvas_init(id){canvas=document.getElementById(id);context=canvas.getContext('2d');}function Canvas_setColor(color){context.fillStyle='rgb('+color.r+','+color.g+','+color.b+')';}function Canvas_setStroke(w, color){context.width=w;context.strokeStyle='rgb('+color.r+','+color.g+','+color.b+')';}function Canvas_setFont(font){context.font=font;}function Canvas_drawLine(x1, y1, x2, y2){context.beginPath();context.moveTo(x1, y1);context.lineTo(x2, y2);context.stroke();}function Canvas_drawRect(x,y,w,h){context.beginPath();context.rect(x,y,w,h);context.stroke();}function Canvas_fillRect(x,y,w,h){context.fillRect(x,y,w,h);}function Canvas_drawString(text, x, y){context.fillText(text, x, y);}function Canvas_width(){return canvas.width;}function Canvas_height(){return canvas.height;}function Canvas_mouseX(){return mousePos.x;}function Canvas_mouseY(){return mousePos.y;}
/*** BEGIN GLOBALS ****/

/*** BEGIN FUNCTIONS ****/
function Main_start(i){
	let func=Main_g;
	Main_derivative(Main_f);
}
function Main_f(x){
	return x*x;
}
function Main_g(x, y){
	return x*y;
}
function Main_derivative(f){
}
Main_start();