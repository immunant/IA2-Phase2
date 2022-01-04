#include <vector>
#include <array>
#include <iostream>
#include <sstream>
#include <ranges>
#include <algorithm>
#include <cassert>

#include "CAbi.h"

struct ParamLocation {
	private:
		ParamLocation(const char* reg) : reg(reg) {}
	public:
		const char* reg;

		static ParamLocation Register(const char* regname) {
			return ParamLocation(regname);
		}
		static ParamLocation Stack() {
			return ParamLocation(nullptr);
		}

		bool is_stack() {
			return reg == nullptr;
		}
		bool is_xmm() {
			return reg != nullptr && reg[0] == 'x' && reg[1] == 'm' && reg[2] == 'm';
		}
		const char* as_str() {
			if (reg) {
				return reg;
			} else {
				return "<stack>";
			}
		}
		operator const char*() {
			return as_str();
		}
};

const std::array<const char*, 6> int_param_reg_order = {"rdi","rsi","rdx","rcx","r8","r9"};
const std::array<const char*, 8> xmms = {"xmm0","xmm1","xmm2","xmm3","xmm4","xmm5", "xmm6", "xmm7"};

const std::array<const char*, 6> int_ret_reg_order = {"rax","rdx"};

/*
compute abi locations for parameters of a C-abi function, given its sequence of argument kinds
*/
auto param_locations(const CAbiSignature& func) -> std::vector<ParamLocation> {
	std::vector<ParamLocation> locs = {};
	size_t ints_used = 0;
	size_t floats_used = 0;
	for (const auto& arg : func.args) {
		switch (arg) {
			case CAbiArgKind::Integral:
				if (ints_used < int_param_reg_order.size()) {
					locs.push_back(ParamLocation::Register(int_param_reg_order[ints_used]));
				} else {
					locs.push_back(ParamLocation::Stack());
				}
				ints_used += 1;
				break;
			case CAbiArgKind::Float:
				if (floats_used < xmms.size()) {
					locs.push_back(ParamLocation::Register(xmms[floats_used]));
				} else {
					locs.push_back(ParamLocation::Stack());
				}
				floats_used += 1;
				break;
		}
	}
	return locs;
}

auto return_locations(const CAbiSignature& func) -> std::vector<ParamLocation> {
	std::vector<ParamLocation> locs = {};
	size_t size_in_qwords = 1;
	switch (func.ret) {
		case CAbiArgKind::Integral:
			locs.push_back(ParamLocation::Register(int_ret_reg_order[0]));
			if (size_in_qwords == 2) {
				locs.push_back(ParamLocation::Register(int_ret_reg_order[1]));
			}
			//locs.push_back(ParamLocation::Stack());
			break;
		case CAbiArgKind::Float:
			//TODO: handle x87 in st0 and complex x87 in st0+st1
			locs.push_back(ParamLocation::Register(xmms[0]));
			if (size_in_qwords == 2) {
				locs.push_back(ParamLocation::Register(xmms[1]));
			}
			break;
	}

	//TODO: do we need to handle __int128/vectors/etc. here?
	assert(size_in_qwords == 1);

	return locs;
}

static void print_locs(const std::vector<ParamLocation>& locs) {
	for (auto loc : locs) {
		std::cout << loc.as_str() << " ";
	}
	std::cout << std::endl;
}

static void add_asm_line(std::stringstream& ss, const std::string& s) {
	ss << "\"" << s << "\\n\"" << std::endl;
}

#define COMMENT_PREFIX "// "

static void add_comment_line(std::stringstream& ss, const std::string& s) {
	ss << COMMENT_PREFIX << s << std::endl;
}

static auto sig_string(const CAbiSignature& sig, const std::string& name) -> std::string {
	std::stringstream ss = {};
	bool first = true;
	ss << name << "(";
	for (auto arg : sig.args) {
		if (!first) {
			ss << ", ";
		}
		switch (arg) {
			case CAbiArgKind::Integral:
				ss << "int";
				break;
			case CAbiArgKind::Float:
				ss << "float";
				break;
		}
	}
	ss << ")";
	return ss.str();
}

auto emit_call_asm(const CAbiSignature& sig, const std::string& name) -> std::string {
	using namespace std::string_literals;

	std::stringstream ss = {};

	//use intel syntax to save endangered percent-signs
	add_asm_line(ss, ".intel_syntax noprefix");
	add_comment_line(ss, "wrapper for "s + sig_string(sig, name) + ":");

	//declare symbol
	ss << "\".global __ia2_" << name << "\\n\"" << std::endl;
	ss << "\"__ia2_" << name << ":\\n\"" << std::endl;

	//save trusted stack ptr to trusted tls
	add_comment_line(ss, "save trusted stack ptr to trusted tls");
	add_asm_line(ss, "mov rax, QWORD PTR ia2_trusted_stackptr@GOTPCREL[rip]");
	add_asm_line(ss, "mov [rax], rsp");

	//switch to untrusted stack
	add_comment_line(ss, "switch to untrusted stack");
	add_asm_line(ss, "mov rsp, QWORD PTR ia2_untrusted_stackptr@GOTPCREL[rip]");
	add_asm_line(ss, "mov rsp, [rsp]");

	//copy stack args to untrusted stack
	add_comment_line(ss, "copy any stack arguments to the untrusted stack");
	//add_asm_line(ss, "mov rax, [ia2_trusted_stackptr]"); // use rax to point at the trusted stack
	auto param_locs = param_locations(sig);
	size_t stack_arg_count = std::ranges::count_if(param_locs, &ParamLocation::is_stack);
	//TODO: emit memcpy(untrusted_stack, trusted_stack, stack_size);
	size_t arg_stack_offset = 16;
	for (auto loc : param_locs) {
		if (loc.is_stack()) {
			add_asm_line(ss, "push qword [rax+"s + std::to_string(arg_stack_offset) + "]");
			arg_stack_offset += 8;
			//XXX: do we have to worry about 128b-vs-64b stack entries here?
		}
	}

	//push any arg regs
	add_comment_line(ss, "save used arg regs as they are needed post-scrubbing");
	for (auto loc : param_locs) {
		if (!loc.is_stack()) {
			if (loc.is_xmm()) {
				add_asm_line(ss, "sub rsp, 16");
				add_asm_line(ss, "movdqu [rsp], "s + loc.as_str());
			} else {
				add_asm_line(ss, "push "s + loc.as_str());
			}
		}
	}

	//call scrub
	add_comment_line(ss, "scrub registers before call");
	add_asm_line(ss, "call __libia2_scrub_registers");

	//change pkru to untrusted
	add_comment_line(ss, "change pkru to untrusted");
	add_asm_line(ss, "call __libia2_untrusted_gate_push");

	//pop arg regs
	add_comment_line(ss, "restore arg regs for call");
	for (auto loc : std::ranges::views::reverse(param_locs)) {
		if (!loc.is_stack()) {
			if (loc.is_xmm()) {
				add_asm_line(ss, "movdqu "s + loc.as_str() + ", [rsp]");
				add_asm_line(ss, "add rsp, 16");
			} else {
				add_asm_line(ss, "pop "s + loc.as_str());
			}
		}
	}

	//call wrapped
	add_comment_line(ss, "call wrapped function");
	add_asm_line(ss, "call "s + name);
	//any prefix of the following may be skipped by untrusted code, so be careful:
	/////////////////////////////////////////////////////////////////////////
	//change pkru to trusted
	add_comment_line(ss, "change pkru to trusted");
	add_asm_line(ss, "call __libia2_untrusted_gate_pop");

	auto return_locs = return_locations(sig);
	#ifdef STACK_RETURNS
	//copy any stack returns to trusted stack
	add_comment_line(ss, "copy any stack return values to trusted stack");
	size_t stack_ret_count = std::ranges::count_if(return_locs, &ParamLocation::is_stack);
	#endif

	add_comment_line(ss, "push return values to trusted stack");
	add_asm_line(ss, "mov rdi, QWORD PTR ia2_trusted_stackptr@GOTPCREL[rip]");
	add_asm_line(ss, "mov rdi, [rdi]"); // read the location of top-of-stack
	//push return regs to trusted stack redzone
	auto pushed_offset = 0;
	for (auto loc : return_locs) {
		if (!loc.is_stack()) {
			pushed_offset += 8;
			add_asm_line(ss, "mov [rdi-"s + std::to_string(pushed_offset) + "], " + loc.as_str());
		}
	}

	//call scrub
	add_comment_line(ss, "scrub registers after call");
	add_asm_line(ss, "call __libia2_scrub_registers");

	//switch to trusted stack, adjusting stack ptr for the regs we saved on it
	add_comment_line(ss, "switch to trusted stack, adjusting stack ptr for any return regs we saved on it");
	add_asm_line(ss, "mov rsp, QWORD PTR ia2_trusted_stackptr@GOTPCREL[rip]");
	add_asm_line(ss, "mov rsp, [rsp]");

	//if any args were pushed to the stack, make room to pop them
	if (pushed_offset > 0)
		add_asm_line(ss, "sub rsp, "s + std::to_string(pushed_offset));

	//pop return regs
	add_comment_line(ss, "pop return regs");
	for (auto loc : return_locs) {
		if (!loc.is_stack()) {
			add_asm_line(ss, "pop "s + loc.as_str());
		}
	}

	//return
	add_comment_line(ss, "return");
	add_asm_line(ss, "ret");

	//reset syntax to AT&T
	add_asm_line(ss, ".att_syntax");

	return ss.str();
}

/*static*/ auto gen_call_asm_test_main() -> int {
	using enum CAbiArgKind;
	auto sig = CAbiSignature {
		.args = {Integral, Integral, Integral},
		.ret = Integral,
		.variadic = false,
	};
	print_locs(param_locations(sig));

	auto sig2 = CAbiSignature {
		.args = {
			Integral, Integral, Integral, Integral, Integral,
			Integral, Integral, Integral, Integral, Integral,
			Integral, Integral, Integral, Integral, Integral,
			Integral, Integral, Integral, Integral, Integral,
			Integral, Integral, Integral, Integral, Integral,
			Integral, Integral, Integral, Integral, Integral,
		},
		.ret = Integral,
		.variadic = false,
	};
	print_locs(param_locations(sig2));

	auto sig3 = CAbiSignature {
		.args = {
			Integral, Integral, Integral, Integral, Integral,
			Integral, Integral, Float, Integral
		},
		.ret = Integral,
		.variadic = false,
	};
	print_locs(param_locations(sig3));
	auto asm_str = emit_call_asm(sig3, "myfunc");
	std::cout << "asm:\n" << asm_str;

	return 0;
}
