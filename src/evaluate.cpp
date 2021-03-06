#include "dada.h"
#include <Rcpp.h>
using namespace Rcpp;

//------------------------------------------------------------------
// Exposes Needleman-Wunsch alignment to R.
//
// @param s1 A \code{character(1)} of DNA sequence 1.
// @param s2 A \code{character(1)} of DNA sequence 2.
// @param score The 4x4 score matrix for nucleotide transitions.
// @param gap_p The gap penalty.
// @param band The band size (-1 turns off banding).
// @param endsfree If TRUE, allow free end gaps.
// 
// @return A \code{character(2)}. The aligned strings.
// 
// [[Rcpp::export]]
Rcpp::CharacterVector C_nwalign(std::string s1, std::string s2, int match, int mismatch, int gap_p, int homo_gap_p, int band, bool endsfree) {
  int i, j;
  char **al;
  // Make integer-ized c-style sequence strings
  char *seq1 = (char *) malloc(s1.size()+1); //E
  char *seq2 = (char *) malloc(s2.size()+1); //E
  if (seq1 == NULL || seq2 == NULL)  Rcpp::stop("Memory allocation failed.");
  nt2int(seq1, s1.c_str());
  nt2int(seq2, s2.c_str());
  // Make  c-style 2d score array
  int c_score[4][4];
  for(i=0;i<4;i++) {
    for(j=0;j<4;j++) {
      if(i==j) { c_score[i][j] = match; }
      else { c_score[i][j] = mismatch; }
    }
  }
  // Perform alignment and convert back to ACGT
  if(endsfree) {
    if(gap_p == homo_gap_p) {
      al = nwalign_endsfree(seq1, seq2, c_score, gap_p, band);
    } else {
      al = nwalign_endsfree_homo(seq1, seq2, c_score, gap_p, homo_gap_p, band);
    }
  } else {
    if(gap_p != homo_gap_p) {
      Rprintf("Warning: A separate homopolymer gap penalty isn't implemented when endsfree=FALSE.\n\tAll gaps will be penalized by the regular gap penalty.\n");
    }
    al = nwalign(seq1, seq2, c_score, gap_p, band);
  }
  int2nt(al[0], al[0]);
  int2nt(al[1], al[1]);
  // Generate R-style return vector
  Rcpp::CharacterVector rval;
  rval.push_back(std::string(al[0]));
  rval.push_back(std::string(al[1]));
  // Clean up
  free(seq1);
  free(seq2);
  free(al[0]);
  free(al[1]);
  free(al);
  return(rval);
}

//------------------------------------------------------------------
// Calculates the number of matches/mismatches/internal_indels in an alignment.
// 
// @param s1 A \code{character(1)} of DNA sequence 1.
// @param s2 A \code{character(1)} of DNA sequence 2.
// 
// @return A named \code{integer(3)}. The count of match, mismatch and indel in the alignment.
//  Ignores indels at the ends of the alignment. 
// 
// [[Rcpp::export]]
Rcpp::IntegerVector C_eval_pair(std::string s1, std::string s2) {
  int match, mismatch, indel, start, end;
  bool s1gap, s2gap;
  if(s1.size() != s2.size()) {
    Rprintf("Warning: Aligned strings are not the same length.\n");
    return R_NilValue;
  }

  // Find start of align (end of initial gapping)
  s1gap = s2gap = true;
  start = -1;
  do {
    start++;
    s1gap = s1gap && (s1[start] == '-');
    s2gap = s2gap && (s2[start] == '-');
  } while((s1gap || s2gap) && start<s1.size());

  // Find end of align (start of terminal gapping)
  s1gap = s2gap = true;
  end = s1.size();
  do {
    end--;
    s1gap = s1gap && (s1[end] == '-');
    s2gap = s2gap && (s2[end] == '-');
  } while((s1gap || s2gap) && end>=start);

  // Count the matches, mismatches, indels within the internal part of alignment.
  match = mismatch = indel = 0;
  for(int i=start;i<=end;i++) {
    if(s1[i]=='-' || s2[i]=='-') {
      indel++;
    } else if(s1[i] == s2[i]) {
      match++;
    } else {
      mismatch++;
    }
  }
  
  Rcpp::IntegerVector rval = Rcpp::IntegerVector::create(_["match"]=match, _["mismatch"]=mismatch, _["indel"]=indel);
  return(rval);
}

//------------------------------------------------------------------
// Calculates the consensus of two sequences (prefer sequence wins mismatches).
// 
// @param s1 A \code{character(1)} of DNA sequence 1.
// @param s2 A \code{character(1)} of DNA sequence 2.
// 
// @return A \code{character(1)} of the consensus DNA sequence.
// 
// [[Rcpp::export]]
Rcpp::CharacterVector C_pair_consensus(std::string s1, std::string s2, int prefer, bool trim_overhang) {
  int i;
  if(s1.size() != s2.size()) {
    Rprintf("Warning: Aligned strings are not the same length.\n");
    return R_NilValue;
  }
  
  char *oseq = (char *) malloc(s1.size()+1); //E
  if (oseq == NULL)  Rcpp::stop("Memory allocation failed.");
  for(i=0;i<s1.size();i++) {
    if(s1[i] == s2[i]) {
      oseq[i] = s1[i];
    } else if(s2[i] == '-') {
      oseq[i] = s1[i];
    } else if(s1[i] == '-') {
      oseq[i] = s2[i];
    } else {
      if(prefer==1) {
        oseq[i] = s1[i]; // s1 wins mismatches
      } else if(prefer==2) {
        oseq[i] = s2[i];
      } else {
        oseq[i] = 'N'; // Should never happen
      }
    }
  }
  // Trim overhangs
  if(trim_overhang) {
    for(i=0;i<s1.size();i++) {
      if(s1[i] != '-') { break; }
      oseq[i] = '-';
    }
    for(i=s1.size()-1;i>=0;i--) {
      if(s2[i] != '-') { break; }
      oseq[i] = '-';
    }
  }
  // Remove remaining gaps
  int j=0;
  for(i=0;i<s1.size();i++) {
    if(oseq[i] != '-') {
      oseq[j]=oseq[i];
      j++;
    }
  }
  oseq[j] = '\0';

  std::string ostr(oseq);
  free(oseq);
  return(ostr);
}

//------------------------------------------------------------------
// Checks a vector of character sequences for whether they are entirely ACGT.
//
// @param seqs A \code{character} of candidate DNA sequences.
// 
// @return A \code{logical}. Whether or not each input character was ACGT only.
// 
// [[Rcpp::export]]
Rcpp::LogicalVector C_isACGT(std::vector<std::string> seqs) {
  unsigned int i, pos, strlen;
  bool justACGT;
  const char *cstr;
  Rcpp::LogicalVector isACGT(seqs.size());
  
  for(i=0; i<seqs.size(); i++) {
    justACGT = true;
    strlen = seqs[i].length();
    cstr = seqs[i].c_str();
    for(pos=0; pos<strlen; pos++) {
      if(!(cstr[pos] == 'A' || cstr[pos] == 'C' || cstr[pos] == 'G' || cstr[pos] == 'T')) {
        justACGT = false;
        break;
      }
    }
    isACGT(i) = justACGT;
  }
  return(isACGT);
}

//------------------------------------------------------------------
//' Generate the kmer-distance and the alignment distance from the
//'   given set of sequences. 
//'
//' @param seqs (Required). Character.
//'  A vector containing all unique sequences in the data set.
//'  Only A/C/G/T allowed.
//'  
//' @param kmer_size (Required). A \code{numeric(1)}. The size of the kmer to test (eg. 5-mer).
//' 
//' @param score (Required). Numeric matrix (4x4).
//' The score matrix used during the alignment. Coerced to integer.
//'
//' @param gap (Required). A \code{numeric(1)} giving the gap penalty for alignment. Coerced to integer.
//'
//' @param band (Required). A \code{numeric(1)} giving the band-size for the NW alignments.
//'
//' @param max_aligns (Required). A \code{numeric(1)} giving the (maximum) number of
//' pairwise alignments to do.
//'
//' @return data.frame
//'
//' @examples
//' derep1 = derepFastq(system.file("extdata", "sam1F.fastq.gz", package="dada2"))
//' kmerdf <- dada2:::evaluate_kmers(getSequences(derep1), 5, getDadaOpt("SCORE_MATRIX"),
//'                                  getDadaOpt("GAP_PENALTY"), 16, 1000)
//' plot(kmerdf$kmer, kmerdf$align)
//' 
// [[Rcpp::export]]
Rcpp::DataFrame evaluate_kmers(std::vector< std::string > seqs, int kmer_size, Rcpp::NumericMatrix score, int gap, int band, unsigned int max_aligns) {
  int i, j, n_iters, stride, minlen, nseqs, len1 = 0, len2 = 0;
  char *seq1, *seq2;

  int c_score[4][4];
  for(i=0;i<4;i++) {
    for(j=0;j<4;j++) {
      c_score[i][j] = (int) score(i,j);
    }
  }
  nseqs = seqs.size();
  
  // Find the kdist/align-dist for max_aligns sequence comparisons
  if(max_aligns < (nseqs * (nseqs-1)/2)) { // More potential comparisons than max
    double foo = 2 * sqrt((double) max_aligns);
    n_iters = (int) foo + 2; // n_iters * (n_iters-1)/2 > max_aligns
    stride = nseqs/n_iters;
  } else {
    max_aligns = (nseqs * (nseqs-1)/2);
    n_iters = nseqs;
    stride = 1;
  }

  unsigned int npairs = 0;
  Rcpp::NumericVector adist(max_aligns);
  Rcpp::NumericVector kdist(max_aligns);
  Sub *sub;
  uint16_t *kv1;
  uint16_t *kv2;

  for(i=0;i<nseqs;i=i+stride) {
    seq1 = intstr(seqs[i].c_str());
    len1 = strlen(seq1);
    kv1 = get_kmer(seq1, kmer_size);
    for(j=i+1;j<nseqs;j=j+stride) {
      seq2 = intstr(seqs[j].c_str());
      len2 = strlen(seq2);
      kv2 = get_kmer(seq2, kmer_size);

      minlen = (len1 < len2 ? len1 : len2);

      sub = al2subs(nwalign_endsfree(seq1, seq2, c_score, gap, band));
      adist[npairs] = ((double) sub->nsubs)/((double) minlen);
      
      kdist[npairs] = kmer_dist(kv1, len1, kv2, len2, kmer_size);
      npairs++;
      free(kv2);
      free(seq2);
      if(npairs >= max_aligns) { break; }
    }
    free(kv1);
    free(seq1);
    if(npairs >= max_aligns) { break; }
  }
  
  if(npairs != max_aligns) {
    Rcpp::Rcout << "Warning: Failed to reach requested number of alignments.\n";
  }
  return Rcpp::DataFrame::create(_["align"] = adist, _["kmer"] = kdist);
}

// [[Rcpp::export]]
Rcpp::NumericVector kmer_dist(std::vector< std::string > s1, std::vector< std::string > s2, int kmer_size) {
  size_t len1 = 0, len2 = 0;
  char *seq1, *seq2;
  
  size_t nseqs = s1.size();
  if(nseqs != s2.size()) { Rcpp::stop("Mismatched numbers of sequences."); }
  
  Rcpp::NumericVector kdist(nseqs);
  uint16_t *kv1;
  uint16_t *kv2;
  
  for(int i=0;i<nseqs;i++) {
    seq1 = intstr(s1[i].c_str());
    len1 = s1[i].size();
    kv1 = get_kmer(seq1, kmer_size);
    seq2 = intstr(s2[i].c_str());
    len2 = s2[i].size();
    kv2 = get_kmer(seq2, kmer_size);
    kdist[i] = kmer_dist(kv1, len1, kv2, len2, kmer_size);
    free(kv2);
    free(seq2);
    free(kv1);
    free(seq1);
  }
  
  return(kdist);
}

// [[Rcpp::export]]
Rcpp::NumericVector kord_dist(std::vector< std::string > s1, std::vector< std::string > s2, int kmer_size, int SSE) {
  size_t len1 = 0, len2 = 0;
  char *seq1, *seq2;
  
  size_t nseqs = s1.size();
  if(nseqs != s2.size()) { Rcpp::stop("Mismatched numbers of sequences."); }
  
  Rcpp::NumericVector kdist(nseqs);
  uint16_t *kord1;
  uint16_t *kord2;
  
  for(int i=0;i<nseqs;i++) {
    seq1 = intstr(s1[i].c_str());
    len1 = s1[i].size();
    kord1 = get_kmer_order(seq1, kmer_size);
    seq2 = intstr(s2[i].c_str());
    len2 = s2[i].size();
    kord2 = get_kmer_order(seq2, kmer_size);
    if(SSE==1) {
      kdist[i] = kord_dist_SSEi(kord1, len1, kord2, len2, kmer_size);
    } else {
      kdist[i] = kord_dist(kord1, len1, kord2, len2, kmer_size);
    }
    free(kord2);
    free(seq2);
    free(kord1);
    free(seq1);
  }
  
  return(kdist);
}

// [[Rcpp::export]]
Rcpp::IntegerVector kmer_matches(std::vector< std::string > s1, std::vector< std::string > s2, int kmer_size) {
  int i,j;
  size_t len1 = 0, len2 = 0;
  size_t klen1 = 0, klen2 = 0, klen_min = 0;
  int matches=0;
  char *seq1, *seq2;
  
  size_t nseqs = s1.size();
  if(nseqs != s2.size()) { Rcpp::stop("Mismatched numbers of sequences."); }
  
  Rcpp::IntegerVector kmatch(nseqs);
  uint16_t *kord1;
  uint16_t *kord2;
  
  for(i=0;i<nseqs;i++) {
    seq1 = intstr(s1[i].c_str());
    len1 = s1[i].size();
    klen1 = len1 - kmer_size + 1;
    kord1 = get_kmer_order(seq1, kmer_size);
    seq2 = intstr(s2[i].c_str());
    len2 = s2[i].size();
    klen2 = len2 - kmer_size + 1;
    kord2 = get_kmer_order(seq2, kmer_size);
    // Calculate matches
    matches=0;
    klen_min = klen1 < klen2 ? klen1 : klen2;
    for(j=0;j<klen_min;j++) {
      if(kord1[j] == kord2[j]) { matches++; }
    }
    kmatch[i] = matches;
    free(kord2);
    free(seq2);
    free(kord1);
    free(seq1);
  }
  
  return(kmatch);
}

// [[Rcpp::export]]
Rcpp::IntegerVector kdist_matches(std::vector< std::string > s1, std::vector< std::string > s2, int kmer_size) {
  int i, j;
  uint16_t dotsum = 0;
  size_t len1 = 0, len2 = 0;
  char *seq1, *seq2;
  int n_kmer = 1 << (2*kmer_size); // 4^k kmers

  size_t nseqs = s1.size();
  if(nseqs != s2.size()) { Rcpp::stop("Mismatched numbers of sequences."); }
  
  Rcpp::IntegerVector kdist_match(nseqs);
  uint16_t *kv1;
  uint16_t *kv2;
  
  for(i=0;i<nseqs;i++) {
    seq1 = intstr(s1[i].c_str());
    len1 = s1[i].size();
    kv1 = get_kmer(seq1, kmer_size);
    seq2 = intstr(s2[i].c_str());
    len2 = s2[i].size();
    kv2 = get_kmer(seq2, kmer_size);
    // code from kmer_dist
    dotsum = 0;
    for(j=0;j<n_kmer;j++) {
      dotsum += (kv1[j] < kv2[j] ? kv1[j] : kv2[j]);
    }
    kdist_match[i] = dotsum;
    free(kv2);
    free(seq2);
    free(kv1);
    free(seq1);
  }
  
  return(kdist_match);
}


// [[Rcpp::export]]
Rcpp::DataFrame C_subpos(std::string s1, std::string s2) {
  unsigned int i=0;
  unsigned int pos0=1; // R-style 1-indexing
  Rcpp::IntegerVector position;
  Rcpp::LogicalVector error;
  
  for(i=0;i<s1.size();i++) {
    if(s1[i] != '-') {
      if(s1[i] != s2[i] && s2[i] != '-') {
        error.push_back(true);
      } else {
        error.push_back(false);
      }
      position.push_back(pos0);
      pos0++;
    }
  }
  
  return(Rcpp::DataFrame::create(_["pos"]=position, _["err"]=error));
}
