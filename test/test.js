class _6 {
	constructor(next, prev, data) {
		this.next=next;
		this.prev=prev;
		this.data=data;
	}
}
class _a {
	constructor(head, tail, size) {
		this.head=head;
		this.tail=tail;
		this.size=size;
	}
}
function _2(){
}
function _e(){
	let _h=new _6();
	let _i=new _6();
	_h.next=_i;
	_i.prev=_h;
	return new _a(_h, _i, 0);
}
