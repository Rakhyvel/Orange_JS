class _2 {
	constructor(d) {
		this.d=d;
	}
}
class _c {
	constructor(next, prev) {
		this.next=next;
		this.prev=prev;
	}
}
class _f {
	constructor(head, tail) {
		this.head=head;
		this.tail=tail;
	}
}
function _4(){
	let _7=_i();
	let _8=_n(_7);
	while(_8!=_r(_7)){
		_8=_v(_8);
	}
}
function _i(){
	let _l=new _c();
	let _m=new _c();
	_l._v=_m;
	_m.prev=_l;
	return new _f(_l, _m);
}
function _n(_o){
	return _o.head._v;
}
function _r(_s){
	return _s.tail;
}
function _v(_w){
	return _w._v;
}
