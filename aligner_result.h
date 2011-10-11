/*
 * Copyright 2011, Ben Langmead <blangmea@jhsph.edu>
 *
 * This file is part of Bowtie 2.
 *
 * Bowtie 2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Bowtie 2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Bowtie 2.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ALIGNER_RESULT_H_
#define ALIGNER_RESULT_H_

#include <utility>
#include <limits>
#include "mem_ids.h"
#include "ref_coord.h"
#include "read.h"
#include "filebuf.h"
#include "ds.h"
#include "edit.h"

typedef int64_t TAlScore;

#define VALID_AL_SCORE(x)   ((x).score_ > std::numeric_limits<TAlScore>::min())
#define VALID_SCORE(x)      ((x) > std::numeric_limits<TAlScore>::min())
#define INVALIDATE_SCORE(x) ((x) = std::numeric_limits<TAlScore>::min())

/**
 * A generic score object for an alignment.  Used for accounting during
 * SW and elsewhere.  Encapsulates the score, the number of N positions
 * and the number gaps in the alignment.
 *
 * The scale for 'score' is such that a perfect alignment score is 0
 * and a score with non-zero penalty is less than 0.  So differences
 * between scores work as expected, but interpreting an individual
 * score (larger is better) as a penalty (smaller is better) requires
 * taking the absolute value.
 */
class AlnScore {

public:

	/**
	 * Gapped scores are invalid until proven valid.
	 */
	inline AlnScore() {
		reset();
		invalidate();
		assert(!valid());
	}
	
	/**
	 * Reset the score.
	 */
	void reset() {
		score_ = ns_ = gaps_ = 0;
	}

	/**
	 * Return an invalid SwScore.
	 */
	inline static AlnScore INVALID() {
		AlnScore s;
		s.invalidate();
		assert(!s.valid());
		return s;
	}
	
	/**
	 * Return true iff this score has a valid value.
	 */
	inline bool valid() const {
		return score_ != std::numeric_limits<TAlScore>::min();
	}

	/**
	 * Make this score invalid (and therefore <= all other scores).
	 */
	inline void invalidate() {
		score_ = std::numeric_limits<TAlScore>::min();
		assert(!valid());
	}
	
	/**
	 * Increment the number of gaps.  If currently invalid, this makes
	 * the score valid with gaps == 1.
	 */
	inline void incNs(int nceil) {
		if(++ns_ > nceil) {
			invalidate();
		}
		assert_lt(ns_, 0x7fffffff);
	}

	/**
	 * Return true iff this score is > score o.
	 * Note: An "invalid" score is <= all other scores.
	 */
	inline bool operator>(const AlnScore& o) const {
		if(!VALID_AL_SCORE(o)) {
			if(!VALID_AL_SCORE(*this)) {
				// both invalid
				return false;
			} else {
				// I'm valid, other is invalid
				return true;
			}
		} else if(!VALID_AL_SCORE(*this)) {
			// I'm invalid, other is valid
			return false;
		}
		return score_ > o.score_;
	}

	/**
	 * Scores are equal iff they're bitwise equal.
	 */
	inline AlnScore& operator=(const AlnScore& o) {
		// Profiling shows many cache misses on following lines
		gaps_  = o.gaps_;
		ns_    = o.ns_;
		score_ = o.score_;
		assert_lt(ns_, 0x7fffffff);
		return *this;
	}

	/**
	 * Scores are equal iff they're bitwise equal.
	 */
	inline bool operator==(const AlnScore& o) const {
		// Profiling shows cache misses on following line
		return VALID_AL_SCORE(*this) && VALID_AL_SCORE(o) && score_ == o.score_;
	}

	/**
	 * Return true iff the two scores are unequal.
	 */
	inline bool operator!=(const AlnScore& o) const {
		return !(*this == o);
	}

	/**
	 * Return true iff this score is >= score o.
	 */
	inline bool operator>=(const AlnScore& o) const {
		if(!VALID_AL_SCORE(o)) {
			if(!VALID_AL_SCORE(*this)) {
				// both invalid
				return false;
			} else {
				// I'm valid, other is invalid
				return true;
			}
		} else if(!VALID_AL_SCORE(*this)) {
			// I'm invalid, other is valid
			return false;
		}
		return score_ >= o.score_;
	}

	/**
	 * Return true iff this score is < score o.
	 */
	inline bool operator<(const AlnScore& o) const {
		return !operator>=(o);
	}

	/**
	 * Return true iff this score is <= score o.
	 */
	inline bool operator<=(const AlnScore& o) const {
		return !operator>(o);
	}

	/**
	 * Calculate difference between two SwScores.
	 */
	inline AlnScore operator-(const AlnScore& o) const {
		if(!VALID_AL_SCORE(*this)) return *this;
		AlnScore s; 
		s.gaps_ = gaps_ - o.gaps_;
		s.ns_ = ns_;
		s.score_ = score_ - o.score_;
		assert_lt(s.ns_, 0x7fffffff);
		return s;
	}

	/**
	 * Calculate sum of two SwScores.
	 */
	inline AlnScore operator+(const AlnScore& o) const {
		if(!VALID_AL_SCORE(*this)) return *this;
		AlnScore s;
		s.gaps_ = gaps_ + o.gaps_;
		s.ns_ = ns_;
		s.score_ = score_ + o.score_;
		assert_lt(s.ns_, 0x7fffffff);
		return s;
	}

	/**
	 * Add given SwScore into this one.
	 */
	inline AlnScore operator+=(const AlnScore& o) {
		if(VALID_AL_SCORE(*this)) {
			gaps_ += o.gaps_;
			score_ += o.score_;
		}
		return (*this);
	}

	/**
	 * Subtract given SwScore from this one.
	 */
	inline AlnScore operator-=(const AlnScore& o) {
		if(VALID_AL_SCORE(*this)) {
			gaps_ -= o.gaps_;
			score_ -= o.score_;
		}
		return (*this);
	}

	/**
	 * Calculate difference between two SwScores.
	 */
	inline AlnScore operator-(int o) const {
		return (*this) + -o;
	}

	/**
	 * Calculate sum of a SwScore and an integer.
	 */
	inline AlnScore operator+(int o) const {
		if(!VALID_AL_SCORE(*this)) return *this;
		AlnScore s;
		s.gaps_ = gaps_;
		s.ns_ = ns_;
		s.score_ = score_ + o;
		assert_lt(s.ns_, 0x7fffffff);
		return s;
	}

	TAlScore score()   const { return  score_; }
	TAlScore penalty() const { return -score_; }
	TAlScore gaps()    const { return  gaps_;  }
	TAlScore ns()      const { return  ns_;    }

	// Score accumulated so far (penalties are subtracted starting at 0)
	TAlScore score_;
	
	// Ns accumulated so far.  An N opposite a non-gap counts as 1 N
	// (even if it's N-to-N)
	TAlScore ns_;
	
	// # gaps encountered so far, unless that number exceeds the
	// target, in which case the score becomes invalid and therefore <=
	// all other scores
	TAlScore gaps_;
};

enum {
	// This alignment is one of a pair of alignments that form a concordant
	// alignment for a read
	ALN_FLAG_PAIR_CONCORD_MATE1 = 1,
	ALN_FLAG_PAIR_CONCORD_MATE2,

	// This alignment is one of a pair of alignments that form a discordant
	// alignment for a read
	ALN_FLAG_PAIR_DISCORD_MATE1,
	ALN_FLAG_PAIR_DISCORD_MATE2,
	
	// This is an unpaired alignment but the read in question is a pair;
	// usually, this happens because the read had no reportable paired-end
	// alignments
	ALN_FLAG_PAIR_UNPAIRED_MATE1,
	ALN_FLAG_PAIR_UNPAIRED_MATE2,

	// This is an unpaired alignment of an unpaired read
	ALN_FLAG_PAIR_UNPAIRED
};

/**
 * Encapsulates some general information about an alignment that doesn't belong
 * in AlnRes.  Specifically:
 *
 * 1. Whether the alignment is paired
 * 2. If it's paried, whether it's concordant or discordant
 * 3. Whether this alignment was found after the paired-end categories were
 *    maxed out
 * 4. Whether the relevant unpaired category was maxed out
 */
class AlnFlags {

public:

	AlnFlags() {
		init(
			ALN_FLAG_PAIR_UNPAIRED,
			false,  // canMax
			false,  // maxed
			false,  // maxedPair
			false,  // nfilt
			false,  // scfilt
			false,  // lenfilt
			false,  // qcfilt
			false,  // mixedMode
			false,  // primary
			false,  // oppAligned
			false); // oppFw
	}

	AlnFlags(
		int pairing,
		bool canMax,
		bool maxed,
		bool maxedPair,
		bool nfilt,
		bool scfilt,
		bool lenfilt,
		bool qcfilt,
		bool mixedMode,
		bool primary,
		bool oppAligned, // opposite mate aligned?
		bool oppFw)      // opposite mate aligned forward?
	{
		init(pairing, canMax, maxed, maxedPair, nfilt, scfilt,
		     lenfilt, qcfilt, mixedMode, primary, oppAligned, oppFw);
	}

	/**
	 * Initialize given values for all settings.
	 */
	void init(
		int pairing,
		bool canMax,
		bool maxed,
		bool maxedPair,
		bool nfilt,
		bool scfilt,
		bool lenfilt,
		bool qcfilt,
		bool mixedMode,
		bool primary,
		bool oppAligned,
		bool oppFw)
	{
		assert_gt(pairing, 0);
		assert_leq(pairing, ALN_FLAG_PAIR_UNPAIRED);
		pairing_    = pairing;
		canMax_     = canMax;
		maxed_      = maxed;
		maxedPair_  = maxedPair;
		nfilt_      = nfilt;
		scfilt_     = scfilt;
		lenfilt_    = lenfilt;
		qcfilt_     = qcfilt;
		mixedMode_  = mixedMode;
		primary_    = primary;
		oppAligned_ = oppAligned;
		oppFw_      = oppFw;
	}

	/**
	 * Return true iff this alignment is from a paired-end read.
	 */
	bool partOfPair() const {
		assert_gt(pairing_, 0);
		return pairing_ < ALN_FLAG_PAIR_UNPAIRED;
	}
	
	/**
	 * Check that the flags are internally consistent.
	 */
	bool repOk() const {
		assert(partOfPair() || !maxedPair_);
		return true;
	}
	
	/**
	 * Print out string representation of YF:i flag for indicating whether and
	 * why the mate was filtered.
	 */
	bool printYF(OutFileBuf& o, bool first) const;

	/**
	 * Print out string representation of YM:i flag for indicating with the
	 * mate per se aligned repetitively.
	 */
	void printYM(OutFileBuf& o) const;

	/**
	 * Print out string representation of YM:i flag for indicating with the
	 * pair containing the mate aligned repetitively.
	 */
	void printYP(OutFileBuf& o) const;

	/**
	 * Print out string representation of these flags.
	 */
	void printYT(OutFileBuf& o) const;

	inline int  pairing()   const { return pairing_; }
	inline bool maxed()     const { return maxed_; }
	inline bool maxedPair() const { return maxedPair_; }

	/**
	 * Return true iff the alignment is not the primary alignment; i.e. not the
	 * first reported alignment for the fragment.
	 */
	inline bool isPrimary() const {
		return primary_;
	}
	
	/**
	 * Set the primary flag.
	 */
	void setPrimary(bool primary) {
		primary_ = primary;
	}
	
	/**
	 * Return whether both paired and unpaired alignments are considered for
	 * pairs & their constituent mates
	 */
	inline bool isMixedMode() const {
		return mixedMode_;
	}
	
	/**
	 * Return true iff the alignment params are such that it's possible for a
	 * read to be suppressed for being repetitive.
	 */
	inline bool canMax() const {
		return canMax_;
	}
	
	/**
	 * Return true iff the alignment was filtered out.
	 */
	bool filtered() const {
		return !nfilt_ || !scfilt_ || !lenfilt_ || !qcfilt_;
	}
	
	/**
	 * Return true iff the read is mate #1 of a pair, regardless of whether it
	 * aligned as a pair.
	 */
	bool readMate1() const {
		return pairing_ == ALN_FLAG_PAIR_CONCORD_MATE1 ||
		       pairing_ == ALN_FLAG_PAIR_DISCORD_MATE1 ||
			   pairing_ == ALN_FLAG_PAIR_UNPAIRED_MATE1;
	}

	/**
	 * Return true iff the read is mate #2 of a pair, regardless of whether it
	 * aligned as a pair.
	 */
	bool readMate2() const {
		return pairing_ == ALN_FLAG_PAIR_CONCORD_MATE2 ||
		       pairing_ == ALN_FLAG_PAIR_DISCORD_MATE2 ||
			   pairing_ == ALN_FLAG_PAIR_UNPAIRED_MATE2;
	}
	
	/**
	 * Return true iff the read aligned as either mate of a concordant pair.
	 */
	bool alignedConcordant() const {
		return pairing_ == ALN_FLAG_PAIR_CONCORD_MATE1 ||
		       pairing_ == ALN_FLAG_PAIR_CONCORD_MATE2;
	}

	/**
	 * Return true iff the read aligned as either mate of a discordant pair.
	 */
	bool alignedDiscordant() const {
		return pairing_ == ALN_FLAG_PAIR_DISCORD_MATE1 ||
		       pairing_ == ALN_FLAG_PAIR_DISCORD_MATE2;
	}
	
	/**
	 * Return true iff the read aligned as either mate of a pair, concordant or
	 * discordant.
	 */
	bool alignedPaired() const {
		return alignedConcordant() && alignedDiscordant();
	}

	/**
	 * Return true iff the read aligned as an unpaired read.
	 */
	bool alignedUnpaired() const {
		return pairing_ == ALN_FLAG_PAIR_UNPAIRED;
	}

	/**
	 * Return true iff the read aligned as an unpaired mate from a paired read.
	 */
	bool alignedUnpairedMate() const {
		return pairing_ == ALN_FLAG_PAIR_UNPAIRED_MATE1 ||
		       pairing_ == ALN_FLAG_PAIR_UNPAIRED_MATE2;
	}

	bool mateAligned() const {
		return oppAligned_;
	}
	
	bool mateFw() const {
		return oppFw_;
	}

protected:

	// See ALN_FLAG_PAIR_* above
	int pairing_;

	// True iff the alignment params are such that it's possible for a read to
	// be suppressed for being repetitive
	bool canMax_;
	
	// This alignment is sampled from among many alignments that, taken
	// together, cause this mate to align non-uniquely
	bool maxed_;
	
	// The paired-end read of which this mate is part has repetitive concordant
	// alignments
	bool maxedPair_;
	
	bool nfilt_;   // read/mate filtered b/c proportion of Ns exceeded ceil
	bool scfilt_;  // read/mate filtered b/c length can't provide min score
	bool lenfilt_; // read/mate filtered b/c less than or equal to seed mms
	bool qcfilt_;  // read/mate filtered by upstream qc
	
	// Whether both paired and unpaired alignments are considered for pairs &
	// their constituent mates
	bool mixedMode_;
	
	// The read is the primary read 
	bool primary_;

	// True iff the opposite mate aligned
	bool oppAligned_;
	// True iff the opposite mate aligned in fw orientation
	bool oppFw_;
};

static inline ostream& operator<<(ostream& os, const AlnScore& o) {
	os << o.score();
	return os;
}

// Forward declaration
class BitPairReference;

// A given AlnRes can be one of these three types
enum {
	ALN_RES_TYPE_UNPAIRED = 1,   // unpaired alignment
	ALN_RES_TYPE_UNPAIRED_MATE1, // mate #1 in pair, aligned unpaired
	ALN_RES_TYPE_UNPAIRED_MATE2, // mate #2 in pair, aligned unpaired
	ALN_RES_TYPE_MATE1,          // mate #1 in paired-end alignment
	ALN_RES_TYPE_MATE2           // mate #2 in paired-end alignment
};

/**
 * Encapsulates an alignment result, including for colorspace alignments.  The
 * result comprises:
 *
 * 1. All the nucleotide edits for both mates ('ned').
 * 2. All "edits" where an ambiguous reference char is resolved to an
 *    unambiguous char ('aed').
 * 3. All the color miscalls (if in colorspace) ('ced').
 * 4. The score for the alginment, including summary information about the
 *    number of gaps and Ns involved.
 * 5. The reference id, strand, and 0-based offset of the leftmost character
 *    involved in the alignment.
 * 6. Information about trimming prior to alignment and whether it was hard or
 *    soft.
 * 7. Information about trimming during alignment and whether it was hard or
 *    soft.  Local-alignment trimming is usually soft when aligning nucleotide
 *    reads and usually hard when aligning colorspace reads.
 *
 * Note that the AlnRes, together with the Read and an AlnSetSumm (*and* the
 * opposite mate's AlnRes and Read in the case of a paired-end alignment),
 * should contain enough information to print an entire alignment record.
 *
 * TRIMMING
 *
 * Accounting for trimming is tricky.  Trimming affects:
 *
 * 1. The values of the trim* and pretrim* fields.
 * 2. The offsets of the Edits in the EList<Edit>s.
 * 3. The read extent, if the trimming is soft.
 * 4. The read extent and the read sequence and length, if trimming is hard.
 *
 * Handling 1. is not too difficult.  2., 3., and 4. are handled in setShape().
 *
 * Another subtlety is that, in colorspace alignment, there can be soft and/or
 * hard trimming of the colorspace sequence, but that trimming "becomes" hard
 * when it is transferred to the decoded nucleotide sequence.  This is because
 * nucleotide positions not adjacent to an aligned color are not decoded, and
 * so must be omitted entirely instead of soft-clipped.
 */
class AlnRes {

public:

	AlnRes() :
		ned_(RES_CAT),
		aed_(RES_CAT),
		ced_(RES_CAT)
	{
		reset();
	}

	/**
	 * Clear all contents.
	 */
	void reset();
	
	/**
	 * Reverse all edit lists.
	 */
	void reverseEdits() {
		ned_.reverse();
		aed_.reverse();
		ced_.reverse();
	}
	
	/**
	 * Invert positions of edits so that they're with respect to the other end
	 * of the alignment.  The assumption is that the .pos fields of the edits
	 * in the ned_/aed_/ced_ structures are offsets with respect to the first
	 * aligned character (i.e. after all trimming).
	 */
	void invertEdits() {
		assert(shapeSet_);
		assert_gt(rdlen_, 0);
		assert_gt(rdrows_, 0);
		Edit::invertPoss(ned_, rdexrows_);
		Edit::invertPoss(aed_, rdexrows_);
		if(color_) {
			Edit::invertPoss(ced_, rdextent_);
		} else {
			assert(ced_.empty());
		}
	}
	
	/**
	 * Return true iff no result has been installed.
	 */
	bool empty() const {
		if(!VALID_AL_SCORE(score_)) {
			assert(ned_.empty());
			assert(aed_.empty());
			assert(ced_.empty());
			assert(!refcoord_.valid());
			assert(!refival_.valid());
			return true;
		} else {
			return false;
		}
	}

	/**
	 * Return the identifier for the reference that the alignment
	 * occurred in.
	 */
	inline TRefId refid() const {
		assert(shapeSet_);
		return refcoord_.ref();
	}

	/**
	 * Return the orientation that the alignment occurred in.
	 */
	inline int orient() const {
		assert(shapeSet_);
		return refcoord_.orient();
	}
	
	/**
	 * Return the 0-based offset of the alignment into the reference
	 * sequence it aligned to.
	 */
	inline TRefOff refoff() const {
		assert(shapeSet_);
		return refcoord_.off();
	}

	/**
	 * Set arguments to coordinates for the upstream-most and downstream-most
	 * reference positions involved in the alignment.
	 */
	inline void getCoords(
		Coord& st,  // out: install starting coordinate here
		Coord& en)  // out: install ending coordinate here
		const
	{
		assert(shapeSet_);
		st.init(refcoord_);
		en.init(refcoord_);
		en.adjustOff(refExtent() - 1);
	}

	/**
	 * Set arguments to coordinates for the upstream-most and downstream-most
	 * reference positions covered by the read taking any read trimming into
	 * account.  I.e. if the upstream-most offset involved in an alignment is
	 * 40 but the read was hard-trimmed by 5 on that end, the inferred
	 * upstream-most covered position is 35.
	 */
	inline void getExtendedCoords(
		Coord& st,  // out: install starting coordinate here
		Coord& en)  // out: install ending coordinate here
		const
	{
		getCoords(st, en);
		// Take trimming into account
		int64_t trim_st  = (fw() ? trim5p_ : trim3p_);
		int64_t trim_en  = (fw() ? trim3p_ : trim5p_);
		trim_st += (fw() ? pretrim5p_ : pretrim3p_);
		trim_en += (fw() ? pretrim3p_ : pretrim5p_);
		st.adjustOff(-trim_st);
		en.adjustOff( trim_st);
	}
	
	/**
	 * Set the upstream-most reference offset involved in the alignment, and
	 * the extent of the alignment (w/r/t the reference)
	 */
	void setShape(
		TRefId  id,          // id of reference aligned to
		TRefOff off,         // offset of first aligned char into ref seq
		bool    fw,          // aligned to Watson strand?
		size_t  rdlen,       // length of read after hard trimming, before soft
		bool    color,       // colorspace alignment?
		bool    pretrimSoft, // whether trimming prior to alignment was soft
		size_t  pretrim5p,   // # poss trimmed form 5p end before alignment
		size_t  pretrim3p,   // # poss trimmed form 3p end before alignment
		bool    trimSoft,    // whether local-alignment trimming was soft
		size_t  trim5p,      // # poss trimmed form 5p end during alignment
		size_t  trim3p);     // # poss trimmed form 3p end during alignment

	/**
	 * Return true iff the reference chars involved in this alignment result
	 * are entirely within with given bounds.
	 */
	bool within(
		TRefId id,
		TRefOff off,
		bool fw,
		size_t extent) const
	{
		if(refcoord_.ref() == id &&
		   refcoord_.off() >= off &&
		   refcoord_.off() + refExtent() <= off + extent &&
		   refcoord_.fw() == fw)
		{
			return true;
		}
		return false;
	}

	/**
	 * Set alignment score for this alignment.
	 */
	void setScore(AlnScore score) {
		score_ = score;
	}

	/**
	 * Set the upstream-most and downstream-most nucleotides.
	 */
	void setNucs(bool fw, int nup, int ndn) {
		assert(color_);
		nuc5p_ = fw ? nup : ndn;
		nuc3p_ = fw ? ndn : nup;
	}
	
	/**
	 * Return the 0-based offset of the leftmost reference position involved in
	 * the alignment.
	 */
	const Coord& refcoord() const {
		return refcoord_;
	}

	/**
	 * Return the 0-based offset of the leftmost reference position involved in
	 * the alignment.
	 */
	const Interval& refival() const {
		return refival_;
	}

	/**
	 * Return the 0-based offset of the leftmost reference position involved in
	 * the alignment.
	 */
	Coord& refcoord() {
		return refcoord_;
	}
	
	/**
	 * Return true if this alignment is to the Watson strand.
	 */
	inline bool fw() const {
		return refcoord_.fw();
	}
	
	AlnScore           score()          const { return score_;    }
	AlnScore           oscore()         const { return oscore_;   }
	EList<Edit>&       ned()                  { return ned_;      }
	EList<Edit>&       aed()                  { return aed_;      }
	EList<Edit>&       ced()                  { return ced_;      }
	const EList<Edit>& ned()            const { return ned_;      }
	const EList<Edit>& aed()            const { return aed_;      }
	const EList<Edit>& ced()            const { return ced_;      }
	size_t             readExtent()     const { return rdextent_; }
	size_t             readExtentRows() const { return rdexrows_; }
	size_t             readLength()     const { return rdlen_;    }

	/**
	 * Return the number of reference nucleotides or colors involved in the
	 * alignment (i.e. the number of characters in the inclusive range from the
	 * first matched-up ref char to the last).  If alignment was in colorspace,
	 * this is the extent measured in colors.
	 */
	size_t refExtent() const {
		return rfextent_;
	}

	/**
	 * Return the number of reference nucleotides in the alignment (i.e. the
	 * number of characters in the inclusive range from the first matched-up
	 * ref char to the last).
	 */
	size_t refNucExtent() const {
		return rfextent_ + (color_ ? 1 : 0);
	}

	/**
	 * Print a CIGAR-string representation of the alignment.  In the
	 * CIGAR-string representation, edit operations are printed in an order
	 * that corresponds to moving left-to-right along the Watson strand.  The
	 * operators indicate one of: match, mismatch, read gap, and reference gap.
	 * With each operator is an associated run length (printed prior to the
	 * operator) indicating how many times in a row that feature occurs.  Hard
	 * and soft trimming are represented with H and S operators, repsectively.
	 */
 	void printCigar(
		bool printColors,     // print CIGAR for colorspace alignment?
		bool exEnds,          // exclude ends nucleotides for decoded nucs?
		bool distinguishMm,   // use =/X instead of just M
		EList<char>& op,      // stick CIGAR operations here
		EList<size_t>& run,   // stick CIGAR run lengths here
		OutFileBuf* o,        // write to this buf if o != NULL
		char* oc) const;      // write to this buf if oc != NULL

	/**
	 * Print a MD:Z:-string representation of the alignment, a la BWA.  In this
	 * representation runs of either matches or reference gaps are represented
	 * by a single number indicating the length of the run.  Mismatches are
	 * indicated by the DNA character that occurs in the reference part of the
	 * mismatch.  Read gaps are indicated by a carat (^) followed by the string
	 * of reference characters that occur in the gap.  The string encodes the
	 * alignment after all trimming.
	 */
 	void printMD(
		bool printColors,     // print CIGAR for colorspace alignment?
		bool exEnds,          // exclude ends nucleotides for decoded nucs?
		EList<char>& op,      // stick operations here
		EList<char>& ch,      // stick reference characters here
		EList<size_t>& run,   // stick run lengths here
		OutFileBuf* o,        // write to this buf if o != NULL
		char* oc) const;      // write to this buf if oc != NULL
	
	/**
	 * Print the sequence for the read that aligned using A, C, G and
	 * T.  This will simply print the read sequence (or its reverse
	 * complement) unless this is a colorspace read and printColors is
	 * false.  In that case, we print the decoded sequence rather than
	 * the original ones.
	 */
 	void printSeq(
		const Read& rd,
		const BTDnaString* dns,
		bool printColors,
		bool exEnds,
		OutFileBuf& o) const;

	/**
	 * Print the quality string for the read that aligned.  This will
	 * simply print the read qualities (or their reverse) unless this
	 * is a colorspace read and printColors is false.  In that case,
	 * we print the decoded qualities rather than the original ones.
	 */
 	void printQuals(
		const Read& rd,
		const BTString* dqs,
		bool printColors,
		bool exEnds,
		OutFileBuf& o) const;
	
	/**
	 * Print a stacked alignment with the reference on top, query on bottom,
	 * and lines connecting matched-up positions.
	 */
	void printStacked(
		const Read& rd,
		std::ostream& o) const
	{
		printStacked(refcoord_.fw() ? rd.patFw : rd.patRc, o);
	}

	/**
	 * Print a stacked alignment with the reference on bottom, query on top,
	 * and lines connecting matched-up positions.
	 */
	void printStacked(
		const BTDnaString& seq,
		std::ostream& o) const
	{
		Edit::printQAlign(o, seq, ned_);
		// Print reference offset below reference string
		o << "^" << std::endl;
		o << "(" << refcoord_.ref() << "," << refcoord_.off() << ")" << std::endl;
	}
	
	/**
	 * Check that alignment score is internally consistent.
	 */
	bool repOk() const {
		assert(refcoord_.repOk());
		assert(refival_.repOk());
		assert(VALID_AL_SCORE(score_) || ned_.empty());
		assert(VALID_AL_SCORE(score_) || aed_.empty());
		assert(VALID_AL_SCORE(score_) || ced_.empty());
		assert(empty() || refcoord_.valid());
		assert(empty() || refival_.valid());
		assert_geq(rdexrows_, rdextent_);
		assert(empty() || rdextent_ > 0);
		assert(empty() || rfextent_ > 0);
		return true;
	}
	
#ifndef NDEBUG
	/**
	 * Assuming this AlnRes is an alignment for 'rd', check that the
	 * alignment and 'rd' are compatible with the corresponding
	 * reference sequence.
	 */
	bool matchesRef(
		const Read& rd,
		const BitPairReference& ref,
		BTDnaString& rf,
		BTDnaString& rdseq,
		BTString& qseq,
		SStringExpandable<char>& raw_refbuf,
		SStringExpandable<uint32_t>& destU32,
		EList<bool>& matches);
#endif
	
	/**
	 * Set information about the alignment parameters that led to this
	 * alignment.
	 */
	void setParams(
		int seedmms,
		int seedlen,
		int seedival,
		int64_t minsc,
		int64_t floorsc)
	{
		seedmms_ = seedmms;
		seedlen_ = seedlen;
		seedival_ = seedival;
		minsc_ = minsc;
	}
	
	// Accessors for alignment parameters
	int     seedmms()    const { return seedmms_;  }
	int     seedlen()    const { return seedlen_;  }
	int     seedival()   const { return seedival_; }
	int64_t minScore()   const { return minsc_;    }
	int64_t floorScore() const { return floorsc_;  }
	bool    color()      const { return color_;    }

	/**
	 * Get the decoded nucleotide sequence 
	 */
	void decodedNucsAndQuals(
		const Read& rd,       // read that led to alignment
		BTDnaString& ns,      // out: decoded nucleotides
		BTString& qs) const;  // out: decoded qualities
	
	/**
	 * Is the ith row from the 5' end of the DP table one of the ones
	 * soft-trimmed away by local alignment? 
	 */
	inline bool trimmedRow5p(size_t i) const {
		return i < trim5p_ || rdrows_ - i - 1 < trim3p_;
	}

	/**
	 * Is the ith character from the 5' end of read sequence one of the ones
	 * soft-trimmed away by local alignment? 
	 */
	inline bool trimmedPos5p(size_t i) const {
		return i < trim5p_ || rdlen_ - i - 1 < trim3p_;
	}

	/**
	 * Is the ith row from the 5' end of the DP table one of the ones that
	 * survived local-alignment soft trimming?
	 */
	inline bool alignedRow5p(size_t i) const {
		return !trimmedRow5p(i);
	}

	/**
	 * Is the ith character from the 5' end of the read sequence one of the
	 * ones that survived local-alignment soft trimming?
	 */
	inline bool alignedPos5p(size_t i) const {
		return !trimmedPos5p(i);
	}
	
	/**
	 * Return true iff this AlnRes and the given AlnRes overlap.  Two AlnRess
	 * overlap if they share a cell in the overall dynamic programming table:
	 * i.e. if there exists a read position s.t. that position in both reads
	 * matches up with the same reference character.  E.g., the following
	 * alignments (drawn schematically as paths through a dynamic programming
	 * table) are redundant:
	 *
	 *  a  b           a  b
	 *  \  \           \  \
	 *   \  \           \  \
	 *    \  \           \  \
	 *     ---\           \  \
	 *         \           ---\---
	 *       ---\              \  \
	 *        \  \              \  \
	 *         \  \              \  \
	 *          \  \              \  \
	 *          a  b              b  a
	 *
	 * We iterate over each read position that hasn't been hard-trimmed, but
	 * only overlaps at positions that have also not been soft-trimmed are
	 * considered.
	 */
	bool overlap(AlnRes& res);
	
	/**
	 * Return true iff this read was unpaired to begin with.
	 */
	inline bool readUnpaired() const {
		assert_gt(type_, 0);
		return type_ == ALN_RES_TYPE_UNPAIRED;
	}

	/**
	 * Return true iff this alignment aligned in an unpaired fashion; not part
	 * of a concordant or discordant pair.
	 */
	inline bool alignedUnpaired() const {
		assert_gt(type_, 0);
		return type_ == ALN_RES_TYPE_UNPAIRED ||
		       type_ == ALN_RES_TYPE_UNPAIRED_MATE1 ||
			   type_ == ALN_RES_TYPE_UNPAIRED_MATE2;
	}

	/**
	 * Return true iff this alignment aligned as mate #1 or mate #2 in a pair,
	 * either concordant or discordant.
	 */
	inline bool alignedPaired() const {
		assert_gt(type_, 0);
		return type_ == ALN_RES_TYPE_MATE1 ||
		       type_ == ALN_RES_TYPE_MATE2;
	}

	/**
	 * Return true iff this read started as mate #1 in a pair.
	 */
	inline bool readMate1() const {
		assert_gt(type_, 0);
		return type_ == ALN_RES_TYPE_MATE1 ||
		       type_ == ALN_RES_TYPE_UNPAIRED_MATE1;
	}

	/**
	 * Return true iff this read aligned as mate #1 in a concordant or
	 * discordant pair.
	 */
	inline bool alignedMate1() const {
		assert_gt(type_, 0);
		return type_ == ALN_RES_TYPE_MATE1;
	}

	/**
	 * Return true iff this alignment aligned as mate #2 in a pair, either
	 * concordant or discordant.
	 */
	inline bool readMate2() const {
		assert_gt(type_, 0);
		return type_ == ALN_RES_TYPE_MATE2 ||
		       type_ == ALN_RES_TYPE_UNPAIRED_MATE2;
	}
	
	/**
	 * Return true iff this read aligned as mate #2 in a concordant or
	 * discordant pair.
	 */
	inline bool alignedMate2() const {
		assert_gt(type_, 0);
		return type_ == ALN_RES_TYPE_MATE2;
	}
	
	/**
	 * Set whether this alignment is unpaired, or is mate #1 or mate #2 in a
	 * paired-end alignment.
	 */
	void setMateParams(
		int type,
		const AlnRes* omate,
		const AlnFlags& flags)
	{
		assert_gt(type, 0);
		type_ = type;
		fraglen_ = 0;
		if(omate != NULL) {
			oscore_ = omate->score_;
			if(flags.alignedConcordant()) {
				setFragmentLength(*omate);
			}
		}
	}
	
	/**
	 * Assuming this alignment and the given alignment are at the extreme ends
	 * of a fragment, return the length of the fragment.  We take all clipping,
	 * both hard and soft, into account here.  Any clipping that occurred
	 * earlier and isn't accounted for within Bowtie2 should be accounted for
	 * by the user in how they set the maximum and minimum fragment length
	 * settings.
	 */
	int64_t setFragmentLength(const AlnRes& omate) {
		Coord st, en;
		Coord ost, oen;
		assert_eq(refid(), omate.refid());
		getExtendedCoords(st, en);
		omate.getExtendedCoords(ost, oen);
		bool imUpstream = st.off() < ost.off();
		TRefOff up = std::min(st.off(), ost.off());
		TRefOff dn = std::max(en.off(), oen.off());
		assert_geq(dn, up);
		fraglen_ = 1 + dn - up;
		if(!imUpstream) {
			fraglen_ = -fraglen_;
		}
		return fraglen_;
	}
	
	/**
	 * Return fragment length inferred by a paired-end alignment, or -1 if the
	 * alignment is not part of a pair.
	 */
	int64_t fragmentLength() const {
		assert_gt(type_, 0);
		return fraglen_;
	}
	
	/**
	 * Initialize new AlnRes.
	 */
	void init(
		size_t             rdlen,           // # chars after hard trimming
		AlnScore           score,           // alignment score
		const EList<Edit>* ned,             // nucleotide edits
		const EList<Edit>* aed,             // ambiguous base resolutions
		const EList<Edit>* ced,             // color edits
		Coord              refcoord,        // leftmost ref pos of 1st al char
		bool               color,           // colorspace?
		int                seedmms      = -1,// # seed mms allowed
		int                seedlen      = -1,// seed length
		int                seedival     = -1,// space between seeds
		int64_t            minsc        = -1,// minimum score for valid aln
		int64_t            floorsc      = -1,// local-alignment floor
		int                nuc5p        = -1,//
		int                nuc3p        = -1,
		bool               pretrimSoft  = false,
		size_t             pretrim5p    = 0, // trimming prior to alignment
		size_t             pretrim3p    = 0, // trimming prior to alignment
		bool               trimSoft     = true,
		size_t             trim5p       = 0, // trimming from alignment
		size_t             trim3p       = 0, // trimming from alignment
		bool               cPretrimSoft = false,
		size_t             cPretrim5p   = 0, // trimming prior to alignment
		size_t             cPretrim3p   = 0, // trimming prior to alignment
		bool               cTrimSoft    = true,
		size_t             cTrim5p      = 0, // trimming from alignment
		size_t             cTrim3p      = 0);// trimming from alignment

	size_t softTrimmed5p() const {
		size_t trim = 0;
		if(pretrimSoft_) {
			trim += pretrim5p_;
		}
		if(trimSoft_) {
			trim += trim5p_;
		}
		return trim;
	}

	size_t softTrimmed3p() const {
		size_t trim = 0;
		if(pretrimSoft_) {
			trim += pretrim3p_;
		}
		if(trimSoft_) {
			trim += trim3p_;
		}
		return trim;
	}

	/**
	 * Set the number of reference Ns covered by the alignment.
	 */
	void setRefNs(size_t refns) {
		refns_ = refns;
	}
	
	/**
	 * Return the number of reference Ns covered by the alignment.
	 */
	size_t refNs() const { return refns_; }
	
	/**
	 * Clip away portions of the alignment that are outside the given bounds.
	 * Clipping is soft if soft == true, hard otherwise.
	 */
	void clipOutside(bool soft, TRefOff refi, TRefOff reff);


	/**
	 * Soft trim bases from the LHS of the alignment.
	 */
	void clipLeft(size_t rd_amt, size_t rf_amt);

	/**
	 * Soft trim bases from the RHS of the alignment.
	 */
	void clipRight(size_t rd_amt, size_t rf_amt);

	/**
	 * In debug mode, we put a copy of the decoded nucleotide sequence here.
	 */
	ASSERT_ONLY(BTDnaString drd);
	
	/**
	 * Return true iff this AlnRes should come before the given AlnRes in a
	 * prioritized list of results.
	 */
	bool operator<(const AlnRes& o) const {
		return score_ > o.score_;
	}

protected:

	/**
	 * Given that rdextent_ and ned_ are already set, calculate rfextent_.
	 */
	void calcRefExtent() {
		assert_gt(rdextent_, 0);
		rfextent_ = rdextent_;
		for(size_t i = 0; i < ned_.size(); i++) {
			if(ned_[i].isRefGap()) rfextent_--;
			if(ned_[i].isReadGap()) rfextent_++;
		}
	}

	bool        shapeSet_;     // true iff setShape() has been called
	size_t      rdlen_;        // length of the original read
	size_t      rdrows_;       // # rows in alignment problem
	AlnScore    score_;        // best SW score found
	AlnScore    oscore_;       // score of opposite mate
	EList<Edit> ned_;          // base edits
	EList<Edit> aed_;          // ambiguous base resolutions
	EList<Edit> ced_;          // color miscalls
	Coord       refcoord_;     // ref coordinates (seq idx, offset, orient)
	Interval    refival_;      // ref interval (coord + length)
	size_t      rdextent_;     // number of read chars involved in alignment
	size_t      rdexrows_;     // number of read rows involved in alignment
	size_t      rfextent_;     // number of ref chars involved in alignment
	int         seedmms_;      // number of mismatches allowed in seed
	int         seedlen_;      // length of seed
	int         seedival_;     // interval between seeds
	int64_t     minsc_;        // minimum score
	int64_t     floorsc_;      // floor score
	bool        color_;        // colorspace alignment?
	int         nuc5p_;        // 5'-most decoded base; clipped if excluding end
	int         nuc3p_;        // 3'-most decoded base; clipped if excluding end
	size_t      refns_;        // # of reference Ns overlapped
	int         type_;         // unpaired or mate #1 or mate #2?
	int64_t     fraglen_;      // inferred fragment length
	
	// A tricky aspect of trimming is that we have to decide what the units are:
	// read positions, reference positions???  We choose read positions here.
	// In other words, if an alignment overhangs the end of the reference and
	// part of the overhanging portion is a reference gap, we have to make sure
	// the trim amount reflects the number of *read characters* to trim
	// including the character opposite the reference gap.
	
	// Nucleotide-sequence trimming
	bool        pretrimSoft_;  // trimming prior to alignment is soft?
	size_t      pretrim5p_;    // # bases trimmed from 5p end prior to alignment
	size_t      pretrim3p_;    // # bases trimmed from 3p end prior to alignment
	bool        trimSoft_;     // trimming by local alignment is soft?
	size_t      trim5p_;       // # bases trimmed from 5p end by local alignment
	size_t      trim3p_;       // # bases trimmed from 3p end by local alignment

	// Colorspace-sequence trimming; only relevant in colorspace
	bool        cPretrimSoft_; // trimming prior to alignment is soft?
	size_t      cPretrim5p_;   // # bases trimmed from 5p end prior to alignment
	size_t      cPretrim3p_;   // # bases trimmed from 3p end prior to alignment
	bool        cTrimSoft_;    // trimming by local alignment is soft?
	size_t      cTrim5p_;      // # bases trimmed from 5p end by local alignment
	size_t      cTrim3p_;      // # bases trimmed from 3p end by local alignment
};

/**
 * Unique ID for a cell in the overall DP table.  This is a helpful concept
 * because of our definition of "redundnant".  Two alignments are redundant iff
 * they have at least one cell in common in the overall DP table.
 */
struct RedundantCell {
	
	RedundantCell() {
		rfid = 0;
		fw = true;
		rfoff = 0;
		rdoff = 0;
	}
	
	RedundantCell(
		TRefId  rfid_,
		bool    fw_,
		TRefOff rfoff_,
		size_t  rdoff_)
	{
		init(rfid_, fw_, rfoff_, rdoff_);
	}
	
	void init(
		TRefId  rfid_,
		bool    fw_,
		TRefOff rfoff_,
		size_t  rdoff_)
	{
		rfid  = rfid_;
		fw    = fw_;
		rfoff = rfoff_;
		rdoff = rdoff_;
	}
	
	/**
	 * Return true iff this RedundantCell is less than the given RedundantCell.
	 */
	inline bool operator<(const RedundantCell& c) const {
		if(rfid  <  c.rfid) return true;
		if(rfid  >  c.rfid) return false;
		if(!fw   &&   c.fw) return true;
		if( fw   &&  !c.fw) return false;
		if(rfoff < c.rfoff) return true;
		if(rfoff > c.rfoff) return false;
		return rdoff < c.rdoff;
	}

	/**
	 * Return true iff this RedundantCell is greater than the given
	 * RedundantCell.
	 */
	inline bool operator>(const RedundantCell& c) const {
		if(rfid  >  c.rfid) return true;
		if(rfid  <  c.rfid) return false;
		if( fw   &&  !c.fw) return true;
		if(!fw   &&   c.fw) return false;
		if(rfoff > c.rfoff) return true;
		if(rfoff < c.rfoff) return false;
		return rdoff > c.rdoff;
	}

	/**
	 * Return true iff this RedundantCell is equal to the given RedundantCell.
	 */
	inline bool operator==(const RedundantCell& c) const {
		return
			rfid  == c.rfid  &&
			fw    == c.fw    &&
			rfoff == c.rfoff &&
			rdoff == c.rdoff;
	}

	TRefId  rfid;  // reference id
	bool    fw;    // orientation
	TRefOff rfoff; // column
	size_t  rdoff; // row
};

/**
 * Encapsulates data structures and routines allowing client to determine
 * whether one alignment is redundant (has a DP cell in common with) with a set
 * of others.
 *
 * Adding cells to and checking cell against this data structure can get rather
 * slow when there are many alignments in play.  Dividing the burden over
 * read-position bins helps some.
 */
class RedundantAlns {

public:

	RedundantAlns(int cat = DP_CAT) : cells_(cat) { }

	/**
	 * Empty the cell database.
	 */
	void reset() { cells_.clear(); }
	
	/**
	 * Initialize and set the list of sets to equal the read length.
	 */
	void init(size_t npos) {
		cells_.resize(npos);
		for(size_t i = 0; i < npos; i++) {
			cells_[i].clear();
		}
	}

	/**
	 * Add all of the cells involved in the given alignment to the database.
	 */
	void add(const AlnRes& res);
	
	/**
	 * Return true iff the given alignment has at least one cell that overlaps
	 * one of the cells in the database.
	 */
	bool overlap(const AlnRes& res);

protected:

	EList<ESet<RedundantCell> > cells_;
};

typedef uint64_t TNumAlns;

/**
 * Encapsulates a concise summary of a set of alignment results for a
 * given pair or mate.  Referring to the fields of this object should
 * provide enough information to print output records for the read.
 */
class AlnSetSumm {

public:

	AlnSetSumm() { reset(); }

	/**
	 * Given an unpaired read (in either rd1 or rd2) or a read pair
	 * (mate 1 in rd1, mate 2 in rd2).
	 */
	explicit AlnSetSumm(
		const Read* rd1,
		const Read* rd2,
		const EList<AlnRes>* rs1,
		const EList<AlnRes>* rs2,
		const EList<AlnRes>* rs1u,
		const EList<AlnRes>* rs2u,
		bool exhausted1,
		bool exhausted2,
		TRefId orefid,
		TRefOff orefoff)
	{
		init(rd1, rd2, rs1, rs2, rs1u, rs2u, exhausted1, exhausted2, 
		     orefid, orefoff);
	}

	explicit AlnSetSumm(
		AlnScore best1,
		AlnScore secbest1,
		AlnScore best2,
		AlnScore secbest2,
		AlnScore bestPaired,
		AlnScore secbestPaired,
		TNumAlns other1,
		TNumAlns other2,
		bool     paired,
		bool     exhausted1,
		bool     exhausted2,
		TRefId   orefid,
		TRefOff  orefoff)
	{
		init(
			best1,
			secbest1,
			best2,
			secbest2,
			bestPaired,
			secbestPaired,
			other1,
			other2,
			paired,
			exhausted1,
			exhausted2,
			orefid,
			orefoff);
	}
	
	/**
	 * Set to uninitialized state.
	 */
	void reset() {
		best1_.invalidate();
		secbest1_.invalidate();
		best2_.invalidate();
		secbest2_.invalidate();
		bestPaired_.invalidate();
		secbestPaired_.invalidate();
		other1_ = other2_ = 0;
		paired_ = false;
		exhausted1_ = exhausted2_ = false;
		orefid_ = -1;
		orefoff_ = -1;
	}
	
	void init(
		const Read* rd1,
		const Read* rd2,
		const EList<AlnRes>* rs1,
		const EList<AlnRes>* rs2,
		const EList<AlnRes>* rs1u,
		const EList<AlnRes>* rs2u,
		bool exhausted1,
		bool exhausted2,
		TRefId orefid,
		TRefOff orefoff);
	
	/**
	 * Initialize given fields.  See constructor for how fields are set.
	 */
	void init(
		AlnScore best1,
		AlnScore secbest1,
		AlnScore best2,
		AlnScore secbest2,
		AlnScore bestPaired,
		AlnScore secbestPaired,
		TNumAlns other1,
		TNumAlns other2,
		bool     paired,
		bool     exhausted1,
		bool     exhausted2,
		TRefId   orefid,
		TRefOff  orefoff)
	{
		best1_         = best1;
		secbest1_      = secbest1;
		best2_         = best2;
		secbest2_      = secbest2;
		bestPaired_    = bestPaired;
		secbestPaired_ = secbestPaired;
		other1_        = other1;
		other2_        = other2;
		paired_        = paired;
		exhausted1_    = exhausted1;
		exhausted2_    = exhausted2;
		orefid_        = orefid;
		orefoff_       = orefoff;
		assert(repOk());
	}
	
	/**
	 * Return true iff there is at least a best alignment
	 */
	bool empty() const {
		assert(repOk());
		return !VALID_AL_SCORE(best1_);
	}
	
	/**
	 * Check that the summary is internally consistent.
	 */
	bool repOk() const {
		assert(other1_ == 0 ||  VALID_AL_SCORE(secbest1_));
		assert(other1_ != 0 || !VALID_AL_SCORE(secbest1_));
		assert(other2_ == 0 ||  VALID_AL_SCORE(secbest2_));
		assert(other2_ != 0 || !VALID_AL_SCORE(secbest2_));
		return true;
	}
	
	AlnScore best1()         const { return best1_;         }
	AlnScore secbest1()      const { return secbest1_;      }
	AlnScore best2()         const { return best2_;         }
	AlnScore secbest2()      const { return secbest2_;      }
	AlnScore bestPaired()    const { return bestPaired_;    }
	AlnScore secbestPaired() const { return secbestPaired_; }
	TNumAlns other1()        const { return other1_;        }
	TNumAlns other2()        const { return other2_;        }
	bool     paired()        const { return paired_;        }
	bool     exhausted1()    const { return exhausted1_;    }
	bool     exhausted2()    const { return exhausted2_;    }
	TRefId   orefid()        const { return orefid_;        }
	TRefOff  orefoff()       const { return orefoff_;       }

	/**
	 *
	 */
	AlnScore best(bool mate1) const { return mate1 ? best1_ : best2_; }

	bool exhausted(bool mate1) const {
		return mate1 ? exhausted1_ : exhausted2_;
	}

	/**
	 * Return the second-best score for the specified mate.  If the alignment
	 * is paired and the specified mate aligns uniquely, return an invalid
	 * second-best score.  This allows us to treat mates separately, so that
	 * repetitive paired-end alignments don't trump potentially unique unpaired
	 * alignments.
	 */
	AlnScore secbestMate(bool mate1) const {
		return mate1 ? secbest1_ : secbest2_;
	}
	
	/**
	 * Return the second-best score for the specified mate.  If the alignment
	 * is paired and the specified mate aligns uniquely, return an invalid
	 * second-best score.  This allows us to treat mates separately, so that
	 * repetitive paired-end alignments don't trump potentially unique unpaired
	 * alignments.
	 */
	AlnScore secbest(bool mate1) const {
		if(paired_) {
			if(mate1) {
				//if(!secbest1_.valid()) {
					return secbest1_;
				//}
			} else {
				//if(!secbest2_.valid()) {
					return secbest2_;
				//}
			}
			//return secbestPaired_;
		} else {
			return mate1 ? secbest1_ : secbest2_;
		}
	}
	
protected:
	
	AlnScore bestPaired_;    // best full-alignment score found for this read
	AlnScore secbestPaired_; // second-best
	AlnScore best1_;         // best full-alignment score found for this read
	AlnScore secbest1_;      // second-best
	AlnScore best2_;         // best full-alignment score found for this read
	AlnScore secbest2_;      // second-best
	TNumAlns other1_;        // # more alignments within N points of second-best
	TNumAlns other2_;        // # more alignments within N points of second-best
	bool     paired_;        // results are paired
	bool     exhausted1_;    // searched exhaustively for mate 1 alignments?
	bool     exhausted2_;    // searched exhaustively for mate 2 alignments?
	TRefId   orefid_;
	TRefOff  orefoff_;
};

#endif
