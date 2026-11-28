// Standalone robustness harness for the bundled liborigin (.opj parser).
//
// Usage: opj_fuzz [-s] file1.opj [file2.opj ...]
//   -s   print a deterministic summary of every parsed project (tables,
//        columns, row counts, value hashes, matrices, graphs, notes, tree).
//        Used to check that hardening patches do not change the result for
//        valid files.
//
// Each file is parsed via OriginFile::parse(). C++ exceptions thrown by the
// parser are caught and counted as "handled" (ImportOPJ catches them too).
// Memory errors / UB are detected by building with
//   -fsanitize=address,undefined
// (see build.sh in this directory).

#include "OriginFile.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>

namespace {

struct Fnv {
	uint64_t h = 1469598103934665603ULL;
	void bytes(const void *p, size_t n) {
		const unsigned char *c = static_cast<const unsigned char *>(p);
		for (size_t i = 0; i < n; ++i) {
			h ^= c[i];
			h *= 1099511628211ULL;
		}
	}
	void dbl(double d) { bytes(&d, sizeof d); }
	void str(const std::string &s) { bytes(s.data(), s.size()); }
};

void hashVariant(Fnv &f, const Origin::variant &v) {
	if (v.type() == Origin::variant::V_DOUBLE) {
		f.dbl(v.as_double());
	} else {
		const char *s = v.as_string();
		f.bytes(s, std::strlen(s));
	}
}

void summarize(const char *path, OriginFile &opj, bool ok) {
	std::printf("FILE %s parse=%d version=%.2f\n", path, ok ? 1 : 0, opj.version());
	std::printf(" datasets=%zu spreads=%zu matrices=%zu excels=%zu functions=%zu graphs=%zu notes=%zu\n",
	            (size_t)opj.datasetCount(), (size_t)opj.spreadCount(), (size_t)opj.matrixCount(),
	            (size_t)opj.excelCount(), (size_t)opj.functionCount(), (size_t)opj.graphCount(),
	            (size_t)opj.noteCount());
	for (size_t s = 0; s < opj.spreadCount(); ++s) {
		const Origin::SpreadSheet &sp = opj.spread(s);
		std::printf(" spread[%zu] name=%s label=%s maxRows=%u sheets=%u cols=%zu\n", s, sp.name.c_str(),
		            sp.label.c_str(), sp.maxRows, sp.sheets, sp.columns.size());
		for (size_t c = 0; c < sp.columns.size(); ++c) {
			const Origin::SpreadColumn &col = sp.columns[c];
			Fnv f;
			for (const auto &v : col.data) hashVariant(f, v);
			std::printf("  col[%zu] name=%s type=%d rows=%zu hash=%016llx cmd=%zu cmt=%zu\n", c,
			            col.name.c_str(), (int)col.type, col.data.size(), (unsigned long long)f.h,
			            col.command.size(), col.comment.size());
		}
	}
	for (size_t m = 0; m < opj.matrixCount(); ++m) {
		const Origin::Matrix &mx = opj.matrix(m);
		std::printf(" matrix[%zu] name=%s sheets=%zu\n", m, mx.name.c_str(), mx.sheets.size());
		for (const auto &sh : mx.sheets) {
			Fnv f;
			for (double d : sh.data) f.dbl(d);
			std::printf("  sheet name=%s %ux%u n=%zu hash=%016llx\n", sh.name.c_str(), sh.rowCount,
			            sh.columnCount, sh.data.size(), (unsigned long long)f.h);
		}
	}
	for (size_t e = 0; e < opj.excelCount(); ++e) {
		const Origin::Excel &ex = opj.excel(e);
		std::printf(" excel[%zu] name=%s sheets=%zu maxRows=%u\n", e, ex.name.c_str(), ex.sheets.size(),
		            ex.maxRows);
	}
	for (size_t i = 0; i < opj.functionCount(); ++i) {
		const Origin::Function &fn = opj.function(i);
		std::printf(" function[%zu] name=%s formula=%s points=%d\n", i, fn.name.c_str(), fn.formula.c_str(),
		            fn.totalPoints);
	}
	for (size_t g = 0; g < opj.graphCount(); ++g) {
		const Origin::Graph &gr = opj.graph(g);
		std::printf(" graph[%zu] name=%s layers=%zu\n", g, gr.name.c_str(), gr.layers.size());
		for (size_t l = 0; l < gr.layers.size(); ++l) {
			const Origin::GraphLayer &ly = gr.layers[l];
			std::printf("  layer[%zu] curves=%zu texts=%zu lines=%zu figures=%zu bitmaps=%zu\n", l,
			            ly.curves.size(), ly.texts.size(), ly.lines.size(), ly.figures.size(),
			            ly.bitmaps.size());
			for (const auto &cv : ly.curves)
				std::printf("   curve x=%s y=%s type=%d\n", cv.xColumnName.c_str(), cv.yColumnName.c_str(),
				            (int)cv.type);
		}
	}
	for (size_t n = 0; n < opj.noteCount(); ++n) {
		const Origin::Note &nt = opj.note(n);
		Fnv f;
		f.str(nt.text);
		std::printf(" note[%zu] name=%s len=%zu hash=%016llx\n", n, nt.name.c_str(), nt.text.size(),
		            (unsigned long long)f.h);
	}
	const tree<Origin::ProjectNode> *pt = opj.project();
	size_t nodes = 0;
	for (auto it = pt->begin(); it != pt->end(); ++it) {
		std::printf(" node depth=%d type=%d name=%s\n", pt->depth(it), (int)it->type, it->name.c_str());
		++nodes;
	}
	std::printf(" treeNodes=%zu resultsLog=%zu\n", nodes, opj.resultsLogString().size());
}

// Touch every parsed object so that sanitizers also see the consumers'
// view of the data (string lengths, variant copies, ...).
void walk(OriginFile &opj) {
	volatile size_t sink = 0;
	for (size_t s = 0; s < opj.spreadCount(); ++s)
		for (const auto &col : opj.spread(s).columns)
			for (const auto &v : col.data) {
				Origin::variant copy(v);
				sink = sink + (copy.type() == Origin::variant::V_STRING ? std::strlen(copy.as_string()) : 1);
			}
	for (size_t n = 0; n < opj.noteCount(); ++n) sink = sink + opj.note(n).text.size();
	for (size_t g = 0; g < opj.graphCount(); ++g)
		for (const auto &ly : opj.graph(g).layers)
			for (const auto &t : ly.texts) sink = sink + t.text.size();
	(void)sink;
}

} // namespace

int main(int argc, char **argv) {
	bool summary = false;
	int ok = 0, failed = 0, exceptions = 0;
	for (int i = 1; i < argc; ++i) {
		if (std::strcmp(argv[i], "-s") == 0) {
			summary = true;
			continue;
		}
		try {
			OriginFile opj(argv[i]);
			bool res = opj.parse();
			walk(opj);
			if (summary) summarize(argv[i], opj, res);
			res ? ++ok : ++failed;
		} catch (const std::exception &e) {
			++exceptions;
			if (summary) std::printf("FILE %s exception: %s\n", argv[i], e.what());
		} catch (...) {
			++exceptions;
			if (summary) std::printf("FILE %s exception: (unknown)\n", argv[i]);
		}
	}
	std::fprintf(stderr, "opj_fuzz: ok=%d failed=%d exceptions=%d\n", ok, failed, exceptions);
	return 0;
}
