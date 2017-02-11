#
# write out some stub C code for limbo modules
#
emit(globals: ref Decl)
{
	for(m := globals; m != nil; m = m.next){
		if(m.store != Dtype || m.ty.kind != Tmodule)
			continue;
		m.ty = usetype(m.ty);
		for(d := m.ty.ids; d != nil; d = d.next){
			d.ty = usetype(d.ty);
			if(d.store == Dglobal || d.store == Dfn)
				modrefable(d.ty);
			if(d.store == Dtype && d.ty.kind == Tadt){
				for(id := d.ty.ids; id != nil; id = id.next){
					id.ty = usetype(id.ty);
					modrefable(d.ty);
				}
			}
		}
	}
	if(emitstub){
		adtstub(globals);
		modstub(globals);
	}
	if(emittab != nil)
		modtab(globals);
	if(emitcode != nil)
		modcode(globals);
	if(emitsbl != nil)
		modsbl(globals);
}

modsbl(globals: ref Decl)
{
	for(d := globals; d != nil; d = d.next)
		if(d.store == Dtype && d.ty.kind == Tmodule && d.sym.name == emitsbl)
			break;

	if(d == nil)
		return;
	bsym = bufio->fopen(sys->fildes(1), Bufio->OWRITE);

	sblmod(d);
	sblfiles();
	n := 0;
	genstart();
	for(id := d.ty.tof.ids; id != nil; id = id.next){
		if(id.sym.name == ".mp")
			continue;
		pushblock();
		id.pc = genrawop(id.src, INOP, nil, nil, nil);
		id.pc.pc = n++;
		popblock();
	}
	firstinst = firstinst.next;
	sblinst(firstinst, n);
#	(adts, nadts) := findadts(globals);
	sblty(adts, nadts);
	fs := array[n] of ref Decl;
	n = 0;
	for(id = d.ty.tof.ids; id != nil; id = id.next){
		if(id.sym.name == ".mp")
			continue;
		fs[n] = id;
		n++;
	}
	sblfn(fs, n);
	sblvar(nil);
}

lowercase(f: string): string
{
	for(i := 0; i < len f; i++)
		if(f[i] >= 'A' && f[i] <= 'Z')
			f[i] += 'a' - 'A';
	return f;
}

modcode(globals: ref Decl)
{
	buf: string;

	if(emitdyn){
		buf = lowercase(emitcode);
		print("#include \"%s.h\"\n", buf);
	}
	else{
		print("#include <memory.h>\n");
		print("#include <disvm.h>\n");
		print("#include <builtin_module.h>\n");
		print("#include \"%smod.h\"\n", emitcode);
		print("using disvm::vm_t;\n");
		print("using disvm::runtime::type_descriptor_t;\n");
		print("using disvm::runtime::vm_registers_t;\n");
		print("using disvm::runtime::vm_frame_base_alloc_t;\n");
		print("using disvm::runtime::byte_t;\n");
		print("using disvm::runtime::word_t;\n");
		print("using disvm::runtime::big_t;\n");
		print("using disvm::runtime::real_t;\n");
		print("using disvm::runtime::pointer_t;\n");
	}
	print("\n");

	for(d := globals; d != nil; d = d.next)
		if(d.store == Dtype && d.ty.kind == Tmodule && d.sym.name == emitcode)
			break;

	if(d == nil)
		return;

	#
	# stub types
	#
	for(id := d.ty.ids; id != nil; id = id.next){
		if(id.store == Dtype && id.ty.kind == Tadt){
			id.ty = usetype(id.ty);
			print("std::shared_ptr<const type_descriptor_t> T_%s;\n", id.sym.name);
		}
	}

	#
	# type maps
	#
	if(emitdyn){
		for(id = d.ty.ids; id != nil; id = id.next)
			if(id.store == Dtype && id.ty.kind == Tadt)
				print("byte_t %s_map[] = %s_%s_map;\n",
					id.sym.name, emitcode, id.sym.name);
	}

	#
	# heap allocation and garbage collection for a type
	#
	if(emitdyn){
		for(id = d.ty.ids; id != nil; id = id.next)
			if(id.store == Dtype && id.ty.kind == Tadt){
				print("\n%s_%s*\n%salloc%s(void)\n{\n vm_alloc_t *a = vm_alloc_t::allocate(T_%s);\n return ::new(a->get_allocation())%s_%s{};\n}\n",
					emitcode, id.sym.name, emitcode, id.sym.name, id.sym.name, emitcode, id.sym.name);
				print("\nvoid\n%sfree%s(vm_alloc_t *a)\n{\n // Customized deallocation\n //%s_%s *t = a->get_allocation<%s_%s>();\n //t->~%s_%s();\n\n // Default deallocation\n disvm::runtime::dec_ref_count_and_free(a);\n}\n",
					emitcode, id.sym.name, emitcode, id.sym.name, emitcode, id.sym.name, emitcode, id.sym.name);
			}
	}

	#
	# initialization function
	#
	if(emitdyn)
		print("\nvoid\n%sinit(void)\n{\n", emitcode);
	else{
		print("\nvoid\n%smodinit(void)\n{\n", emitcode);
		print(" disvm::runtime::builtin::register_module_exports(%s_PATH, %smodtab, %smodlen);\n", emitcode, emitcode, emitcode);
	}
	for(id = d.ty.ids; id != nil; id = id.next)
		if(id.store == Dtype && id.ty.kind == Tadt){
			if(emitdyn)
				print(" T_%s = type_descriptor_t::create(%s_%s_size, sizeof(%s_map), %s_map);\n",
					id.sym.name, emitcode, id.sym.name, emitcode, id.sym.name);
			else
				print(" T_%s = type_descriptor_t::create(sizeof(%s_%s), sizeof(%s_%s_map), %s_%s_map);\n",
					id.sym.name, id.dot.sym.name, id.sym.name, id.dot.sym.name, id.sym.name, id.dot.sym.name, id.sym.name);
		}
	print("}\n");

	#
	# end function
	#
	if(emitdyn){
		print("\nvoid\n%send(void)\n{\n}\n", emitcode);
	}

	#
	# stub functions
	#
	for(id = d.ty.tof.ids; id != nil; id = id.next){
		print("\nvoid\n%s_%s(vm_registers_t &r, vm_t &vm)\n{\n auto &fp = r.stack.peek_frame()->base<F_%s_%s>();\n",
			id.dot.sym.name, id.sym.name,
			id.dot.sym.name, id.sym.name);
		if(id.ty.tof != tnone && tattr[id.ty.tof.kind].isptr){
			print("\n //disvm::runtime::dec_ref_count_and_free(vm_alloc_t::from_allocation(*fp.ret));\n //*fp.ret = nullptr;\n");
		}
		print(" throw vm_system_exception{ \"Function not implemented\" };\n");
		print("}\n");
	}

	if(emitdyn)
		print("\n#include \"%smod.h\"\n", buf);
}

modtab(globals: ref Decl)
{
	for(d := globals; d != nil; d = d.next){
		if(d.store == Dtype && d.ty.kind == Tmodule && d.sym.name == emittab){
			n := 0;
			print("disvm::runtime::builtin::vm_runtab_t %smodtab[]={\n", d.sym.name);
			for(id := d.ty.tof.ids; id != nil; id = id.next){
				n++;
				print(" \"");
				if(id.dot != d)
					print("%s.", id.dot.sym.name);
				print("%s\",0x%ux,%s_%s,", id.sym.name, sign(id),
					id.dot.sym.name, id.sym.name);
				if(id.ty.varargs != byte 0)
					print("0,0,{0},");
				else{
					md := mkdesc(idoffsets(id.ty.ids, MaxTemp, MaxAlign), id.ty.ids);
					print("%d,%d,%s,", md.size, md.nmap, mapconv(md));
				}
				print("\n");
			}
			print(" 0\n};\n\n");
			print("const %smodlen = %d\n", d.sym.name, n);
		}
	}
}

#
# produce activation records for all the functions in modules
#
modstub(globals: ref Decl)
{
	for(d := globals; d != nil; d = d.next){
		if(d.store != Dtype || d.ty.kind != Tmodule)
			continue;
		arg := 0;
		for(id := d.ty.tof.ids; id != nil; id = id.next){
			s := id.dot.sym.name + "_" + id.sym.name;
			if(emitdyn && id.dot.dot != nil)
				s = id.dot.dot.sym.name + "_" + s;
			print("void %s(vm_registers_t &, vm_t &);\nstruct F_%s : public vm_frame_base_alloc_t\n{\n",
				s, s);
			if(id.ty.tof != tnone)
				print(" %s* ret;\n", ctypeconv(id.ty.tof));
			else
				print(" word_t noret;\n");
			print(" byte_t temps[%d];\n", MaxTemp-NREG*IBY2WD);
			offset := MaxTemp;
			for(m := id.ty.ids; m != nil; m = m.next){
				p := "";
				if(m.sym != nil)
					p = m.sym.name;
				else
					p = "arg"+string arg;

				#
				# explicit pads for structure alignment
				#
				t := m.ty;
				(offset, nil) = stubalign(offset, t.align, nil);
				if(offset != m.offset)
					yyerror("module stub must not contain data objects");
					# fatal("modstub bad offset");
				print(" %s %s;\n", ctypeconv(t), p);
				arg++;
				offset += t.size;
#ZZZ need to align?
			}
			if(id.ty.varargs != byte 0)
				print(" byte_t vargs;\n");
			print("};\n");
		}
		for(id = d.ty.ids; id != nil; id = id.next)
			if(id.store == Dconst)
				constub(id);
	}
}

chanstub(in: string, id: ref Decl)
{
	print("using %s_%s = %s;\n", in, id.sym.name, ctypeconv(id.ty.tof));
	desc := mktdesc(id.ty.tof);
	print("const byte_t %s_%s_map = %s;\n", in, id.sym.name, mapconv(desc));
}

#
# produce c structs for all adts
#
adtstub(globals: ref Decl)
{
	t, tt: ref Type;
	m, d, id: ref Decl;

	for(m = globals; m != nil; m = m.next){
		if(m.store != Dtype || m.ty.kind != Tmodule)
			continue;
		for(d = m.ty.ids; d != nil; d = d.next){
			if(d.store != Dtype)
				continue;
			t = usetype(d.ty);
			d.ty = t;
			s := dotprint(d.ty.decl, '_');
			case d.ty.kind{
			Tadt =>
				;
			Tint or
			Tbyte or
			Treal or
			Tbig or
			Tfix =>
				print("using %s = %s;\n", ctypeconv(t), s);
			}
		}
	}
	for(m = globals; m != nil; m = m.next){
		if(m.store != Dtype || m.ty.kind != Tmodule)
			continue;
		for(d = m.ty.ids; d != nil; d = d.next){
			if(d.store != Dtype)
				continue;
			t = d.ty;
			if(t.kind == Tadt || t.kind == Ttuple && t.decl.sym != anontupsym){
				if(t.tags != nil){
					pickadtstub(t);
					continue;
				}
				s := dotprint(t.decl, '_');
				print("struct %s\n{\n", s);

				offset := 0;
				for(id = t.ids; id != nil; id = id.next){
					if(id.store == Dfield){
						tt = id.ty;
						(offset, nil) = stubalign(offset, tt.align, nil);
						if(offset != id.offset)
							fatal("adtstub bad offset");
						print(" %s %s;\n", ctypeconv(tt), id.sym.name);
						offset += tt.size;
					}
				}
				if(t.ids == nil){
					print(" char dummy[1];\n");
					offset = 1;
				}
				(offset, nil)= stubalign(offset, t.align, nil);
#ZZZ
(offset, nil) = stubalign(offset, IBY2WD, nil);
				if(offset != t.size && t.ids != nil)
					fatal("adtstub: bad size");
				print("};\n");

				for(id = t.ids; id != nil; id = id.next)
					if(id.store == Dconst)
						constub(id);

				for(id = t.ids; id != nil; id = id.next)
					if(id.ty.kind == Tchan)
						chanstub(s, id);

				desc := mktdesc(t);
				if(offset != desc.size && t.ids != nil)
					fatal("adtstub: bad desc size");
				print("const byte_t %s_map[] = %s;\n", s, mapconv(desc));
#ZZZ
if(0)
				print("struct %s_check {int s[2*(sizeof(%s)==%s_size)-1];};\n", s, s, s);
			}else if(t.kind == Tchan)
				chanstub(m.sym.name, d);
		}
	}
}

#
# emit an expicit pad field for aligning emitted c structs
# according to limbo's definition
#
stubalign(offset: int, a: int, s: string): (int, string)
{
	x := offset & (a-1);
	if(x == 0)
		return (offset, s);
	x = a - x;
	if(s != nil)
		s += sprint("byte_t _pad%d[%d]; ", offset, x);
	else
		print(" byte_t _pad%d[%d];\n", offset, x);
	offset += x;
	if((offset & (a-1)) || x >= a)
		fatal("compiler stub misalign");
	return (offset, s);
}

constub(id: ref Decl)
{
	s := id.dot.sym.name + "_" + id.sym.name;
	case id.ty.kind{
	Tbyte =>
		print("const byte_t %s = %d;\n", s, int id.init.c.val & 16rff);
	Tint or
	Tfix =>
		print("const word_t %s = %#x;\n", s, int id.init.c.val);
	Tbig =>
		print("const big_t %s %bd;\n", s, id.init.c.val);
	Treal =>
		print("const real_t %s = %.16g;\n", s, id.init.c.rval);
	Tstring =>
		print("const char *%s = \"%s\";\n", s, id.init.decl.sym.name);
	}
}

mapconv(d: ref Desc): string
{
	s := "{";
	for(i := 0; i < d.nmap; i++)
		s += "0x" + hex(int d.map[i], 0) + ",";
	if(i == 0)
		s += "0";
	s += "}";
	return s;
}

dotprint(d: ref Decl, dot: int): string
{
	s : string;
	if(d.dot != nil){
		s = dotprint(d.dot, dot);
		s[len s] = dot;
	}
	if(d.sym == nil)
		return s;
	return s + d.sym.name;
}

ckindname := array[Tend] of
{
	Tnone =>	"void",
	Tadt =>		"struct",
	Tadtpick =>	"?adtpick?",
	Tarray =>	"vm_array_t",
	Tbig =>		"big_t",
	Tbyte =>	"byte_t",
	Tchan =>	"vm_channel_t",
	Treal =>	"real_t",
	Tfn =>		"?fn?",
	Tint =>		"word_t",
	Tlist =>	"vm_list_t",
	Tmodule =>	"vm_module_ref_t",
	Tref =>		"?ref?",
	Tstring =>	"vm_string_t",
	Ttuple =>	"?tuple?",
	Texception => "?exception",
	Tfix => "word_t",
	Tpoly => "void*",

	Tainit =>	"?ainit?",
	Talt =>		"?alt?",
	Tany =>		"void*",
	Tarrow =>	"?arrow?",
	Tcase =>	"?case?",
	Tcasel =>	"?casel?",
	Tcasec =>	"?casec?",
	Tdot =>		"?dot?",
	Terror =>	"?error?",
	Tgoto =>	"?goto?",
	Tid =>		"?id?",
	Tiface =>	"?iface?",
	Texcept => "?except?",
	Tinst =>	"?inst?",
};

ctypeconv(t: ref Type): string
{
	if(t == nil)
		return "void";
	s := "";
	case t.kind{
	Terror =>
		return "type error";
	Tref =>
		return "/* " + ctypeconv(t.tof) + " */ pointer_t";
	Tint or
	Tbig or
	Treal or
	Tbyte or
	Tnone or
	Tany or
	Tfix or
	Tpoly =>
		return ckindname[t.kind];
	Tarray or
	Tlist or
	Tstring or
	Tchan or
	Tmodule =>
		return "/* " + ckindname[t.kind] + " */ pointer_t";
	Tadtpick =>
		return ctypeconv(t.decl.dot.ty);
	Tadt or
	Ttuple =>
		if(t.decl.sym != anontupsym)
			return dotprint(t.decl, '_');
		s += "struct { ";
		offset := 0;
		for(id := t.ids; id != nil; id = id.next){
			tt := id.ty;
			(offset, s) = stubalign(offset, tt.align, s);
			if(offset != id.offset)
				fatal("ctypeconv tuple bad offset");
			s += ctypeconv(tt);
			s += " ";
			s += id.sym.name;
			s += "; ";
			offset += tt.size;
		}
		(offset, s) = stubalign(offset, t.align, s);
		if(offset != t.size)
			fatal(sprint("ctypeconv tuple bad t=%s size=%d offset=%d", typeconv(t), t.size, offset));
		s += "}";
	* =>
		fatal("no C equivalent for type " + string t.kind);
	}
	return s;
}

pickadtstub(t: ref Type)
{
	tt: ref Type;
	desc: ref Desc;
	id, tg: ref Decl;
	ok: byte;
	offset, tgoffset: int;

	buf := dotprint(t.decl, '_');
	offset = 0;
	for(tg = t.tags; tg != nil; tg = tg.next)
		print("#define %s_%s %d\n", buf, tg.sym.name, offset++);
	print("struct %s\n{\n", buf);
	print("	int	pick;\n");
	offset = IBY2WD;
	for(id = t.ids; id != nil; id = id.next){
		if(id.store == Dfield){
			tt = id.ty;
			(offset, nil) = stubalign(offset, tt.align, nil);
			if(offset != id.offset)
				fatal("pickadtstub bad offset");
			print("	%s	%s;\n", ctypeconv(tt), id.sym.name);
			offset += tt.size;
		}
	}
	print("	union{\n");
	for(tg = t.tags; tg != nil; tg = tg.next){
		tgoffset = offset;
		print("		struct{\n");
		for(id = tg.ty.ids; id != nil; id = id.next){
			if(id.store == Dfield){
				tt = id.ty;
				(tgoffset, nil) = stubalign(tgoffset, tt.align, nil);
				if(tgoffset != id.offset)
					fatal("pickadtstub bad offset");
				print("			%s	%s;\n", ctypeconv(tt), id.sym.name);
				tgoffset += tt.size;
			}
		}
		if(tg.ty.ids == nil)
			print("			char	dummy[1];\n");
		print("		} %s;\n", tg.sym.name);
	}
	print("	} u;\n");
	print("};\n");

	for(id = t.ids; id != nil; id = id.next)
		if(id.store == Dconst)
			constub(id);

	for(id = t.ids; id != nil; id = id.next)
		if(id.ty.kind == Tchan)
			chanstub(buf, id);

	for(tg = t.tags; tg != nil; tg = tg.next){
		ok = tg.ty.tof.ok;
		tg.ty.tof.ok = OKverify;
		sizetype(tg.ty.tof);
		tg.ty.tof.ok = OKmask;
		desc = mktdesc(tg.ty.tof);
		tg.ty.tof.ok = ok;
		print("#define %s_%s_size %d\n", buf, tg.sym.name, tg.ty.size);
		print("#define %s_%s_map %s\n", buf, tg.sym.name, mapconv(desc));
	}
}
