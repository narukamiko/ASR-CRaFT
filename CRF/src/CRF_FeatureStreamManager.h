#ifndef CRF_FEATURESTREAMMANAGER_H_
#define CRF_FEATURESTREAMMANAGER_H_
#include <QuickNet.h>
#include <iostream>

#include "CRF.h"
#include "CRF_InFtrStream_RandPresent.h"
#include "CRF_InLabStream_RandPresent.h"
#include "CRF_FeatureStream.h"



class CRF_FeatureStreamManager
{
private:
	int debug;
	const char* dbgname;
	char* filename;
	const char* format;
	char* hardtarget_filename;
	size_t hardtarget_window_offset;
	size_t width;
	size_t first_ftr;
	size_t num_ftrs;
	size_t window_extent;
	size_t window_offset;
	size_t window_len;
	int delta_order;
	int delta_win;
	char* train_sent_range;
	char* cv_sent_range;
	FILE* normfile;
	int norm_mode;
	double norm_am;
	double norm_av;
	size_t train_cache_frames;
	int train_cache_seed;
	seqtype train_seq_type;
	QNUInt32 rseed;
public:
	CRF_FeatureStreamManager(int debug, const char* debug_name,
									char* ftr_fname, const char* ftr_file_fmt, char* ht_fname, size_t ht_offset,
									size_t ftr_width, size_t first_ftr, size_t num_ftrs,
									size_t win_ext, size_t win_off, size_t win_len,
									int delta_o, int delta_w,
									char* trn_rng, char* cv_rng,
									FILE* nfile, int n_mode, double n_am, double n_av, seqtype ts,
									QNUInt32 rseed=0);
	
	virtual ~CRF_FeatureStreamManager();
	void create();
	void display();
	void join(CRF_FeatureStreamManager*);
	void setUtt(QNUInt32);
	void setFiles(char*, const char*, char*, size_t, size_t, size_t);
	void setNorm(FILE*, int, double, double);
	void setRanges(char*, char*, size_t, int);
	void setWindow(size_t, size_t, size_t);
	void setDeltas(int,int);
	size_t getNumFtrs();
	int setDebug(int, const char*);
	CRF_FeatureStream* trn_stream;
	CRF_FeatureStream* cv_stream;

	
};

#endif /*QN_FEATURESTREAM_H_*/
