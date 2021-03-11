class _2 {
	constructor(d) {
		this.d=d;
	}
}
class _9 {
	constructor(next, prev) {
		this.next=next;
		this.prev=prev;
	}
}
class _c {
	constructor(head, tail) {
		this.head=head;
		this.tail=tail;
	}
}
function _4(){
	let _7=_f();
}
function _f(){
	let _i=new _9();
	let _j=new _9();
	_i._s=_j;
	_j.prev=_i;
	return new _c(_i, _j);
}
function _k(list){
	return _l.head._s;
}
function _o(list){
	return _p.tail;
}
function _s(node){
	return _t._s;
}
