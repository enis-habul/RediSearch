#include "../buffer.h"
#include "../index.h"
#include "../inverted_index.h"
#include "../index_result.h"
#include "../query_parser/tokenizer.h"
#include "../rmutil/alloc.h"
#include "../spec.h"
#include "../tokenize.h"
#include "../varint.h"
#include "../rmutil/alloc.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <float.h>
#include <gtest/gtest.h>
#include <vector>
#include <cstdint>

class IndexTest : public ::testing::Test {};

static RSOffsetVector offsetsFromVVW(const VarintVectorWriter *vvw) {
  RSOffsetVector ret = {0};
  ret.data = VVW_GetByteData(vvw);
  ret.len = VVW_GetByteLength(vvw);
  return ret;
}

TEST_F(IndexTest, testVarint) {
  VarintVectorWriter *vw = new VarintVectorWriter(8);
  uint32_t expected[5] = {10, 1000, 1020, 10000, 10020};
  for (int i = 0; i < 5; i++) {
    VVW_Write(vw, expected[i]);
  }

  // VVW_Write(vw, 100);
  // printf("%ld %ld\n", BufferLen(vw->bw.buf), vw->bw.buf->cap);
  VVW_Truncate(vw);

  RSOffsetVector vec = offsetsFromVVW(vw);
  // Buffer_Seek(vw->bw.buf, 0);
  RSOffsetIterator it = RSOffsetVector_Iterate(&vec, NULL);
  int x = 0;
  uint32_t n = 0;
  while (RS_OFFSETVECTOR_EOF != (n = it.Next(it.ctx, NULL))) {
    auto curexp = expected[x++];
    ASSERT_EQ(curexp, n) << "Wrong number decoded";
    // printf("%d %d\n", x, n);
  }
  it.Free(it.ctx);
  VVW_Free(vw);
}

TEST_F(IndexTest, testDistance) {
  VarintVectorWriter *vw = new VarintVectorWriter(8);
  VarintVectorWriter *vw2 = new VarintVectorWriter(8);
  VarintVectorWriter *vw3 = new VarintVectorWriter(8);
  VVW_Write(vw, 1);
  VVW_Write(vw, 9);
  VVW_Write(vw, 13);
  VVW_Write(vw, 16);
  VVW_Write(vw, 22);

  VVW_Write(vw2, 4);
  VVW_Write(vw2, 7);
  VVW_Write(vw2, 32);

  VVW_Write(vw3, 20);
  VVW_Write(vw3, 25);

  VVW_Truncate(vw);
  VVW_Truncate(vw2);

  TermResult *tr1(NULL, 1);
  tr1->docId = 1;
  tr1->offsets = offsetsFromVVW(vw);

  TermResult *tr2(NULL, 1);
  tr2->docId = 1;
  tr2->offsets = offsetsFromVVW(vw2);

  IntersectResult *res(2, 1);
  res->AddChild(tr1);
  res->AddChild(tr2);

  int delta = res->MinOffsetDelta();
  ASSERT_EQ(2, delta);

  ASSERT_EQ(0, res->IsWithinRange(0, 0));
  ASSERT_EQ(0, res->IsWithinRange(0, 1));
  ASSERT_EQ(0, res->IsWithinRange(1, 1));
  ASSERT_EQ(1, res->IsWithinRange(1, 0));
  ASSERT_EQ(1, res->IsWithinRange(2, 1));
  ASSERT_EQ(1, res->IsWithinRange(2, 0));
  ASSERT_EQ(1, res->IsWithinRange(3, 1));
  ASSERT_EQ(1, res->IsWithinRange(4, 0));
  ASSERT_EQ(1, res->IsWithinRange(4, 1));
  ASSERT_EQ(1, res->IsWithinRange(5, 1));

  TermResult *tr3(NULL, 1);
  tr3->docId = 1;
  tr3->offsets = offsetsFromVVW(vw3);
  res->AddChild(tr3);

  delta = res->MinOffsetDelta(res);
  ASSERT_EQ(7, delta);

  // test merge iteration
  RSOffsetIterator it = res->IterateOffsets();
  uint32_t expected[] = {1, 4, 7, 9, 13, 16, 20, 22, 25, 32, RS_OFFSETVECTOR_EOF};

  uint32_t rc;
  int i = 0;
  do {
    rc = it.Next(it.ctx, NULL);
    ASSERT_EQ(rc, (expected[i++]));
  } while (rc != RS_OFFSETVECTOR_EOF);
  it.Free(it.ctx);

  delete res;
  VVW_Free(vw);
  VVW_Free(vw2);
  VVW_Free(vw3);
}

class IndexFlagsTest : public testing::TestWithParam<int> {};

TEST_P(IndexFlagsTest, testRWFlags) {
  IndexFlags indexFlags = (IndexFlags)GetParam();
  InvertedIndex *idx = new InvertedIndex(indexFlags, 1);

  IndexEncoder enc = InvertedIndex_GetEncoder(indexFlags);
  ASSERT_TRUE(enc != NULL);

  for (size_t i = 0; i < 200; i++) {
    // if (i % 10000 == 1) {
    //     printf("iw cap: %ld, iw size: %d, numdocs: %d\n", w->cap, IW_Len(w),
    //     w->ndocs);
    // }

    ForwardIndexEntry h;
    h.docId = i;
    h.fieldMask = 1;
    h.freq = (1 + i % 100) / (float)101;

    h.vw = new VarintVectorWriter(8);
    for (int n = 0; n < i % 4; n++) {
      VVW_Write(h.vw, n);
    }
    VVW_Truncate(h.vw);

    InvertedIndex_WriteForwardIndexEntry(idx, enc, &h);

    // printf("doc %d, score %f offset %zd\n", h.docId, h.docScore, w->bw.buf->offset);
    VVW_Free(h.vw);
  }

  ASSERT_EQ(200, idx->numDocs);
  ASSERT_EQ(2, idx->size);
  ASSERT_EQ(199, idx->lastId);

  // IW_MakeSkipIndex(w, NewMemoryBuffer(8, BUFFER_WRITE));

  //   for (int x = 0; x < w->skipIdx.len; x++) {
  //     printf("Skip entry %d: %d, %d\n", x, w->skipIdx.entries[x].docId,
  //     w->skipIdx.entries[x].offset);
  //   }
  // printf("iw cap: %ld, iw size: %ld, numdocs: %d\n", w->bw.buf->cap, IW_Len(w), w->ndocs);

  for (int xx = 0; xx < 1; xx++) {
    // printf("si: %d\n", si->len);
    IndexReader *ir = new TermIndexReader(idx, NULL, RS_FIELDMASK_ALL, NULL, 1);  //
    IndexResult *h = NULL;

    int n = 0;
    int rc;
    while (!ir->atEnd_) {
      if ((rc = IR_Read(ir, &h)) == INDEXREAD_EOF) {
        break;
      }
      ASSERT_EQ(INDEXREAD_OK, rc);
      ASSERT_EQ(h->docId, n);
      n++;
    }
    // for (int z= 0; z < 10; z++) {
    // clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_time);

    // IR_SkipTo(ir, 900001, &h);

    // clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_time);
    // long diffInNanos = end_time.tv_nsec - start_time.tv_nsec;

    // printf("Time elapsed: %ldnano\n", diffInNanos);
    // //IR_Free(ir);
    // }
    // IndexResult_Free(&h);
    IR_Free(ir);
  }
}

INSTANTIATE_TEST_CASE_P(IndexFlagsP, IndexFlagsTest, ::testing::Range(1, 32));

InvertedIndex *createIndex(int size, int idStep) {
  InvertedIndex *idx = new InvertedIndex((IndexFlags)(INDEX_DEFAULT_FLAGS), 1);

  IndexEncoder enc = InvertedIndex_GetEncoder(idx->flags);
  t_docId id = idStep;
  for (int i = 0; i < size; i++) {
    // if (i % 10000 == 1) {
    //     printf("iw cap: %ld, iw size: %d, numdocs: %d\n", w->cap, IW_Len(w),
    //     w->ndocs);
    // }
    ForwardIndexEntry h;
    h.docId = id;
    h.fieldMask = 1;
    h.freq = 1;
    h.term = "hello";
    h.len = 5;

    h.vw = new VarintVectorWriter(8);
    for (int n = idStep; n < idStep + i % 4; n++) {
      VVW_Write(h.vw, n);
    }

    InvertedIndex_WriteForwardIndexEntry(idx, enc, &h);
    VVW_Free(h.vw);

    id += idStep;
  }

  // printf("BEFORE: iw cap: %ld, iw size: %zd, numdocs: %d\n", w->bw.buf->cap,
  //        IW_Len(w), w->ndocs);

  return idx;
}

int printIntersect(void *ctx, IndexResult *hits, int argc) {
  printf("intersect: %llu\n", (unsigned long long)hits[0].docId);
  return 0;
}

TEST_F(IndexTest, testReadIterator) {
  InvertedIndex *idx = createIndex(10, 1);

  IndexReader *r1 = new TermIndexReader(idx, NULL, RS_FIELDMASK_ALL, NULL, 1);  //

  IndexResult *h = NULL;

  IndexIterator *it = NewReadIterator(r1);
  int i = 1;
  while (IITER_HAS_NEXT(it)) {
    if (it->Read(it->ctx, &h) == INDEXREAD_EOF) {
      break;
    }

    // printf("Iter got %d\n", h.docId);
    ASSERT_EQ(h->docId, i);
    i++;
  }
  ASSERT_EQ(11, i);

  it->Free(it);

  // IndexResult_Free(&h);
  InvertedIndex_Free(idx);
}

TEST_F(IndexTest, testUnion) {
  InvertedIndex *w = createIndex(10, 2);
  InvertedIndex *w2 = createIndex(10, 3);
  IndexReader *r1 = new TermIndexReader(w, NULL, RS_FIELDMASK_ALL, NULL, 1);   //
  IndexReader *r2 = new TermIndexReader(w2, NULL, RS_FIELDMASK_ALL, NULL, 1);  //

  // printf("Reading!\n");
  IndexIterator **irs = (IndexIterator **)calloc(2, sizeof(IndexIterator *));
  irs[0] = NewReadIterator(r1);
  irs[1] = NewReadIterator(r2);

  IndexIterator *ui = NewUnionIterator(irs, 2, NULL, 0, 1);
  IndexResult *h;
  int expected[] = {2, 3, 4, 6, 8, 9, 10, 12, 14, 15, 16, 18, 20, 21, 24, 27, 30};
  int i = 0;
  while (ui->Read(ui->ctx, &h) != INDEXREAD_EOF) {
    // printf("%d <=> %d\n", h.docId, expected[i]);
    ASSERT_EQ(expected[i], h->docId);
    i++;

    IndexResult *copy = new IndexResult(*h);
    ASSERT_TRUE(copy != NULL);
    ASSERT_TRUE(copy != h);
    ASSERT_TRUE(copy->isCopy);

    ASSERT_EQ(copy->docId, h->docId);
    ASSERT_EQ(copy->type, h->type);

    // printf("%d, ", h.docId);
  }

  ui->Free(ui);
  InvertedIndex_Free(w);
  InvertedIndex_Free(w2);
}

TEST_F(IndexTest, testWeight) {
  InvertedIndex *w = createIndex(10, 1);
  InvertedIndex *w2 = createIndex(10, 2);
  IndexReader *r1 = new TermIndexReader(w, NULL, RS_FIELDMASK_ALL, NULL, 0.5);  //
  IndexReader *r2 = new TermIndexReader(w2, NULL, RS_FIELDMASK_ALL, NULL, 1);   //

  // printf("Reading!\n");
  IndexIterator **irs = (IndexIterator **)calloc(2, sizeof(IndexIterator *));
  irs[0] = NewReadIterator(r1);
  irs[1] = NewReadIterator(r2);

  IndexIterator *ui = NewUnionIterator(irs, 2, NULL, 0, 0.8);
  IndexResult *h = NULL;
  int expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 16, 18, 20};
  int i = 0;
  while (ui->Read(ui->ctx, &h) != INDEXREAD_EOF) {
    // printf("%d <=> %d\n", h.docId, expected[i]);
    ASSERT_EQ(h->docId, expected[i++]);
    ASSERT_EQ(h->weight, 0.8);
    if (h->agg.numChildren == 2) {
      ASSERT_EQ(h->agg.children[0]->weight, 0.5);
      ASSERT_EQ(h->agg.children[1]->weight, 1);
    } else {
      if (i <= 10) {
        ASSERT_EQ(h->agg.children[0]->weight, 0.5);
      } else {
        ASSERT_EQ(h->agg.children[0]->weight, 1);
      }
    }
  }

  ui->Free(ui);
  // IndexResult_Free(&h);
  InvertedIndex_Free(w);
  InvertedIndex_Free(w2);
}

TEST_F(IndexTest, testNot) {
  InvertedIndex *w = createIndex(16, 1);
  // not all numbers that divide by 3
  InvertedIndex *w2 = createIndex(10, 3);
  IndexReader *r1 = new TermIndexReader(w, NULL, RS_FIELDMASK_ALL, NULL, 1);   //
  IndexReader *r2 = new TermIndexReader(w2, NULL, RS_FIELDMASK_ALL, NULL, 1);  //

  // printf("Reading!\n");
  IndexIterator **irs = (IndexIterator **)calloc(2, sizeof(IndexIterator *));
  irs[0] = NewReadIterator(r1);
  irs[1] = NewNotIterator(NewReadIterator(r2), w2->lastId, 1);

  IndexIterator *ui = NewIntersecIterator(irs, 2, NULL, RS_FIELDMASK_ALL, -1, 0, 1);
  IndexResult *h = NULL;
  int expected[] = {1, 2, 4, 5, 7, 8, 10, 11, 13, 14, 16};
  int i = 0;
  while (ui->Read(ui->ctx, &h) != INDEXREAD_EOF) {
    // printf("%d <=> %d\n", h->docId, expected[i]);
    ASSERT_EQ(expected[i++], h->docId);
    // printf("%d, ", h.docId);
  }

  ui->Free(ui);
  // IndexResult_Free(&h);
  InvertedIndex_Free(w);
  InvertedIndex_Free(w2);
}

TEST_F(IndexTest, testPureNot) {
  InvertedIndex *w = createIndex(10, 3);

  IndexReader *r1 = new TermIndexReader(w, NULL, RS_FIELDMASK_ALL, NULL, 1);  //
  printf("last id: %llu\n", (unsigned long long)w->lastId);

  IndexIterator *ir = NewNotIterator(NewReadIterator(r1), w->lastId + 5, 1);

  IndexResult *h = NULL;
  int expected[] = {1,  2,  4,  5,  7,  8,  10, 11, 13, 14, 16, 17, 19,
                    20, 22, 23, 25, 26, 28, 29, 31, 32, 33, 34, 35};
  int i = 0;
  while (ir->Read(ir->ctx, &h) != INDEXREAD_EOF) {

    // printf("%d <=> %d\n", h->docId, expected[i]);
    ASSERT_EQ(expected[i++], h->docId);
  }
  ir->Free(ir);
  InvertedIndex_Free(w);
}

// Note -- in test_index.c, this test was never actually run!
TEST_F(IndexTest, DISABLED_testOptional) {
  InvertedIndex *w = createIndex(16, 1);
  // not all numbers that divide by 3
  InvertedIndex *w2 = createIndex(10, 3);
  IndexReader *r1 = new TermIndexReader(w, NULL, RS_FIELDMASK_ALL, NULL, 1);   //
  IndexReader *r2 = new TermIndexReader(w2, NULL, RS_FIELDMASK_ALL, NULL, 1);  //

  // printf("Reading!\n");
  IndexIterator **irs = (IndexIterator **)calloc(2, sizeof(IndexIterator *));
  irs[0] = NewReadIterator(r1);
  irs[1] = NewOptionalIterator(NewReadIterator(r2), w2->lastId, 1);

  IndexIterator *ui = NewIntersecIterator(irs, 2, NULL, RS_FIELDMASK_ALL, -1, 0, 1);
  IndexResult *h = NULL;

  int i = 1;
  while (ui->Read(ui->ctx, &h) != INDEXREAD_EOF) {
    // printf("%d <=> %d\n", h->docId, i);
    ASSERT_EQ(i, h->docId);
    if (i > 0 && i % 3 == 0) {
      ASSERT_EQ(1, h->agg.children[1]->freq);
    } else {
      ASSERT_EQ(0, h->agg.children[1]->freq);
    }
    // printf("%d, ", h.docId);
  }

  ui->Free(ui);
  // IndexResult_Free(&h);
  InvertedIndex_Free(w);
  InvertedIndex_Free(w2);
}

TEST_F(IndexTest, testNumericInverted) {

  InvertedIndex *idx = new InvertedIndex(Index_StoreNumeric, 1);

  for (int i = 0; i < 75; i++) {
    size_t sz = InvertedIndex_WriteNumericEntry(idx, i + 1, (double)(i + 1));
    // printf("written %zd bytes\n", sz);

    ASSERT_TRUE(sz > 1);
  }
  ASSERT_EQ(75, idx->lastId);

  // printf("written %zd bytes\n", idx->blocks[0]->DataLen());

  IndexReader *ir = NewNumericReader(NULL, idx, NULL);
  IndexIterator *it = NewReadIterator(ir);
  IndexResult *res;
  t_docId i = 1;
  while (INDEXREAD_EOF != it->Read(it->ctx, &res)) {
    // printf("%d %f\n", res->docId, res->num.value);

    ASSERT_EQ(i++, res->docId);
    ASSERT_EQ(res->num.value, (float)res->docId);
  }
  InvertedIndex_Free(idx);
  it->Free(it);
}

TEST_F(IndexTest, testNumericVaried) {
  InvertedIndex *idx = new InvertedIndex(Index_StoreNumeric, 1);

  static const double nums[] = {0,          0.13,          0.001,     -0.1,     1.0,
                                5.0,        4.323,         65535,     65535.53, 32768.432,
                                1LLU << 32, -(1LLU << 32), 1LLU << 40};
  static const size_t numCount = sizeof(nums) / sizeof(double);

  for (size_t i = 0; i < numCount; i++) {
    size_t sz = InvertedIndex_WriteNumericEntry(idx, i + 1, nums[i]);
    ASSERT_GT(sz, 1);
    // printf("[%lu]: Stored %lf\n", i, nums[i]);
  }

  IndexReader *ir = NewNumericReader(NULL, idx, NULL);
  IndexIterator *it = NewReadIterator(ir);
  IndexResult *res;

  for (size_t i = 0; i < numCount; i++) {
    // printf("Checking i=%lu. Expected=%lf\n", i, nums[i]);
    int rv = it->Read(it->ctx, &res);
    ASSERT_NE(INDEXREAD_EOF, rv);
    ASSERT_LT(fabs(nums[i] - res->num.value), 0.01);
  }

  ASSERT_EQ(INDEXREAD_EOF, it->Read(it->ctx, &res));
  InvertedIndex_Free(idx);
  it->Free(it);
}

typedef struct {
  double value;
  size_t size;
} encodingInfo;
static const encodingInfo infos[] = {
    {0, 2},                    // 0
    {1, 2},                    // 1
    {63, 3},                   // 2
    {-1, 3},                   // 3
    {-63, 3},                  // 4
    {64, 3},                   // 5
    {-64, 3},                  // 6
    {255, 3},                  // 7
    {-255, 3},                 // 8
    {65535, 4},                // 9
    {-65535, 4},               // 10
    {16777215, 5},             // 11
    {-16777215, 5},            // 12
    {4294967295, 6},           // 13
    {-4294967295, 6},          // 14
    {4294967295 + 1, 7},       // 15
    {4294967295 + 2, 7},       // 16
    {549755813888.0, 7},       // 17
    {549755813888.0 + 2, 7},   // 18
    {549755813888.0 - 23, 7},  // 19
    {-549755813888.0, 7},      // 20
    {1503342028.957225, 10},   // 21
    {42.4345, 6},              // 22
    {(float)0.5, 6},           // 23
    {DBL_MAX, 10},             // 24
    {UINT64_MAX >> 12, 9},     // 25
    {INFINITY, 2},             // 26
    {-INFINITY, 2}             // 27
};

TEST_F(IndexTest, testNumericEncoding) {
  static const size_t numInfos = sizeof(infos) / sizeof(infos[0]);
  InvertedIndex *idx = new InvertedIndex(Index_StoreNumeric, 1);
  // printf("TestNumericEncoding\n");

  for (size_t ii = 0; ii < numInfos; ii++) {
    // printf("\n[%lu]: Expecting Val=%lf, Sz=%lu\n", ii, infos[ii].value, infos[ii].size);
    size_t sz = InvertedIndex_WriteNumericEntry(idx, ii + 1, infos[ii].value);
    ASSERT_EQ(infos[ii].size, sz);
  }

  IndexReader *ir = NewNumericReader(NULL, idx, NULL);
  IndexIterator *it = NewReadIterator(ir);
  IndexResult *res;

  for (size_t ii = 0; ii < numInfos; ii++) {
    // printf("\nReading [%lu]\n", ii);

    int rc = it->Read(it->ctx, &res);
    ASSERT_NE(rc, INDEXREAD_EOF);
    // printf("%lf <-> %lf\n", infos[ii].value, res->num.value);
    if (fabs(infos[ii].value) == INFINITY) {
      ASSERT_EQ(infos[ii].value, res->num.value);
    } else {
      ASSERT_LT(fabs(infos[ii].value - res->num.value), 0.01);
    }
  }

  InvertedIndex_Free(idx);
  it->Free(it);
}

TEST_F(IndexTest, testAbort) {

  InvertedIndex *w = createIndex(1000, 1);
  IndexReader *r = new TermIndexReader(w, NULL, RS_FIELDMASK_ALL, NULL, 1);  //

  IndexIterator *it = NewReadIterator(r);
  int n = 0;
  IndexResult *res;
  while (INDEXREAD_EOF != it->Read(it->ctx, &res)) {
    if (n == 50) {
      it->Abort(it->ctx);
    }
    n++;
  }
  ASSERT_EQ(51, n);
  it->Free(it);
  InvertedIndex_Free(w);
}

TEST_F(IndexTest, testIntersection) {

  InvertedIndex *w = createIndex(100000, 4);
  InvertedIndex *w2 = createIndex(100000, 2);
  IndexReader *r1 = new TermIndexReader(w, NULL, RS_FIELDMASK_ALL, NULL, 1);   //
  IndexReader *r2 = new TermIndexReader(w2, NULL, RS_FIELDMASK_ALL, NULL, 1);  //

  IndexIterator **irs = (IndexIterator **)calloc(2, sizeof(IndexIterator *));
  irs[0] = NewReadIterator(r1);
  irs[1] = NewReadIterator(r2);

  int count = 0;
  IndexIterator *ii = NewIntersecIterator(irs, 2, NULL, RS_FIELDMASK_ALL, -1, 0, 1);

  IndexResult *h;

  uint32_t topFreq = 0;
  while (ii->Read(ii->ctx, &h) != INDEXREAD_EOF) {
    ASSERT_EQ(h->type, RSResultType_Intersection);
    // ASSERT_TRUE(h->IsAggregate());
    ASSERT_TRUE(h->HasOffsets());
    topFreq = topFreq > h->freq ? topFreq : h->freq;

    IndexResult *copy = new IndexResult(*h);
    ASSERT_TRUE(copy != NULL);
    ASSERT_TRUE(copy != h);
    ASSERT_TRUE(copy->isCopy == 1);

    ASSERT_TRUE(copy->docId == h->docId);
    ASSERT_TRUE(copy->type == RSResultType_Intersection);
    ASSERT_EQ((count * 2 + 2) * 2, h->docId);
    ASSERT_EQ(count * 2 + 2, h->freq);
    ++count;
  }

  // int count = IR_Intersect(r1, r2, onIntersect, &ctx);

  // printf("%d intersections in %lldms, %.0fns per iteration\n", count,
  // TimeSampler_DurationMS(&ts),
  // 1000000 * TimeSampler_IterationMS(&ts));
  // printf("top freq: %f\n", topFreq);
  ASSERT_EQ(count, 50000);
  ASSERT_EQ(topFreq, 100000.0);

  ii->Free(ii);
  // IndexResult_Free(&h);
  InvertedIndex_Free(w);
  InvertedIndex_Free(w2);
}

TEST_F(IndexTest, testBuffer) {
  // TEST_START();
  Buffer b = {0};
  Buffer_Init(&b, 2);
  BufferWriter w = NewBufferWriter(&b);
  ASSERT_TRUE(w.buf->cap == 2) << "Wrong capacity";
  ASSERT_TRUE(w.buf->data != NULL);
  ASSERT_TRUE(Buffer_Offset(w.buf) == 0);
  ASSERT_TRUE(w.buf->data == w.pos);

  const char *x = "helololoolo";
  size_t l = Buffer_Write(&w, (void *)x, strlen(x) + 1);

  ASSERT_TRUE(l == strlen(x) + 1);
  ASSERT_TRUE(Buffer_Offset(w.buf) == l);
  ASSERT_EQ(Buffer_Capacity(w.buf), 14);

  l = w.WriteVarint(1337654);
  ASSERT_TRUE(l == 3);
  ASSERT_EQ(Buffer_Offset(w.buf), 15);
  ASSERT_EQ(Buffer_Capacity(w.buf), 17);

  Buffer_Truncate(w.buf, 0);

  ASSERT_TRUE(Buffer_Capacity(w.buf) == 15);

  BufferReader br(w.buf);
  ASSERT_TRUE(br.pos == 0);

  char *y = (char *)malloc(strlen(x) + 1);
  l = Buffer_Read(&br, y, strlen(x) + 1);
  ASSERT_TRUE(l == strlen(x) + 1);

  ASSERT_TRUE(strcmp(y, x) == 0);
  ASSERT_TRUE(BufferReader_Offset(&br) == l);

  free(y);

  int n = ReadVarint(&br);
  ASSERT_TRUE(n == 1337654);

  Buffer_Free(w.buf);
}

typedef struct {
  int num;
  char **expected;

} tokenContext;

int tokenFunc(void *ctx, const Token *t) {
  tokenContext *tx = (tokenContext *)ctx;
  int ret = strncmp(t->tok, tx->expected[tx->num++], t->tokLen);
  EXPECT_TRUE(ret == 0);
  EXPECT_TRUE(t->pos > 0);
  return 0;
}

// int testTokenize() {
//   char *txt = strdup("Hello? world...   ? -WAZZ@UP? שלום");
//   tokenContext ctx = {0};
//   const char *expected[] = {"hello", "world", "wazz", "up", "שלום"};
//   ctx.expected = (char **)expected;

//   tokenize(txt, &ctx, tokenFunc, NULL, 0, DefaultStopWordList(), 0);
//   ASSERT_TRUE(ctx.num == 5);

//   free(txt);

//   return 0;
// }

// int testForwardIndex() {

//   Document doc = NewDocument(NULL, 1, 1, "english");
//   doc.docId = 1;
//   doc.fields[0] = N
//   ForwardIndex *idx(doc);
//   char *txt = strdup("Hello? world...  hello hello ? __WAZZ@UP? שלום");
//   tokenize(txt, 1, 1, idx, ForwardIndex::TokenFunc);

//   return 0;
// }

TEST_F(IndexTest, testIndexSpec) {
  const char *title = "title", *body = "body", *foo = "foo", *bar = "bar", *name = "name";
  const char *args[] = {"STOPWORDS", "2",      "hello", "world",    "SCHEMA", title,
                        "text",      "weight", "0.1",   body,       "text",   "weight",
                        "2.0",       foo,      "text",  "sortable", bar,      "numeric",
                        "sortable",  name,     "text",  "nostem"};
  QueryError err = {QUERY_OK};
  IndexSpec *s = new IndexSpec("idx", args, sizeof(args) / sizeof(const char *), &err);
  ASSERT_FALSE(err.HasError()) << err.GetError();
  ASSERT_TRUE(s);
  ASSERT_TRUE(s->numFields == 5);
  ASSERT_TRUE(s->stopwords != NULL);
  ASSERT_TRUE(s->stopwords != DefaultStopWordList());
  ASSERT_TRUE(s->flags & Index_StoreFieldFlags);
  ASSERT_TRUE(s->flags & Index_StoreTermOffsets);
  ASSERT_TRUE(s->flags & Index_HasCustomStopwords);

  ASSERT_TRUE(S->IsStopWord("hello", 5));
  ASSERT_TRUE(S->IsStopWord("world", 5));
  ASSERT_TRUE(!S->IsStopWord("werld", 5));

  const FieldSpec *f = s->GetField(body, strlen(body));
  ASSERT_TRUE(f != NULL);
  ASSERT_TRUE(f->IsFieldType(INDEXFLD_T_FULLTEXT));
  ASSERT_STREQ(f->name, body);
  ASSERT_EQ(f->ftWeight, 2.0);
  ASSERT_EQ(FIELD_BIT(f), 2);
  ASSERT_EQ(f->options, 0);
  ASSERT_EQ(f->sortIdx, -1);

  f = s->GetField(title, strlen(title));
  ASSERT_TRUE(f != NULL);
  ASSERT_TRUE(f->IsFieldType(INDEXFLD_T_FULLTEXT));
  ASSERT_TRUE(strcmp(f->name, title) == 0);
  ASSERT_TRUE(f->ftWeight == 0.1);
  ASSERT_TRUE(FIELD_BIT(f) == 1);
  ASSERT_TRUE(f->options == 0);
  ASSERT_TRUE(f->sortIdx == -1);

  f = s->GetField(foo, strlen(foo));
  ASSERT_TRUE(f != NULL);
  ASSERT_TRUE(f->IsFieldType(INDEXFLD_T_FULLTEXT));
  ASSERT_TRUE(strcmp(f->name, foo) == 0);
  ASSERT_TRUE(f->ftWeight == 1);
  ASSERT_TRUE(FIELD_BIT(f) == 4);
  ASSERT_TRUE(f->options == FieldSpec_Sortable);
  ASSERT_TRUE(f->sortIdx == 0);

  f = s->GetField(bar, strlen(bar));
  ASSERT_TRUE(f != NULL);
  ASSERT_TRUE(f->IsFieldType(INDEXFLD_T_NUMERIC));

  ASSERT_TRUE(strcmp(f->name, bar) == 0);
  ASSERT_TRUE(f->options == FieldSpec_Sortable);
  ASSERT_TRUE(f->sortIdx == 1);
  ASSERT_TRUE(s->GetField("fooz", 4) == NULL);

  f = s->GetField(name, strlen(name));
  ASSERT_TRUE(f != NULL);
  ASSERT_TRUE(f->IsFieldType(INDEXFLD_T_FULLTEXT));
  ASSERT_TRUE(strcmp(f->name, name) == 0);
  ASSERT_TRUE(f->ftWeight == 1);
  ASSERT_TRUE(FIELD_BIT(f) == 8);
  ASSERT_TRUE(f->options == FieldSpec_NoStemming);
  ASSERT_TRUE(f->sortIdx == -1);

  ASSERT_TRUE(s->sortables != NULL);
  ASSERT_TRUE(s->sortables->len == 2);
  int rc = s->GetFieldSortingIndex(foo, strlen(foo));
  ASSERT_EQ(0, rc);
  rc = S->GetFieldSortingIndex(bar, strlen(bar));
  ASSERT_EQ(1, rc);
  rc = S->GetFieldSortingIndex(title, strlen(title));
  ASSERT_EQ(-1, rc);

  delete s;

  err.ClearError();
  const char *args2[] = {
      "NOOFFSETS", "NOFIELDS", "SCHEMA", title, "text",
  };
  s = new IndexSpec("idx", args2, sizeof(args2) / sizeof(const char *), &err);
  ASSERT_FALSE(err.HasError()) << err.GetError();
  ASSERT_TRUE(s);
  ASSERT_TRUE(s->numFields == 1);

  ASSERT_TRUE(!(s->flags & Index_StoreFieldFlags));
  ASSERT_TRUE(!(s->flags & Index_StoreTermOffsets));
  delete s;

  // User-reported bug
  const char *args3[] = {"SCHEMA", "ha", "NUMERIC", "hb", "TEXT", "WEIGHT", "1", "NOSTEM"};
  err.ClearError();
  s = new IndexSpec("idx", args3, sizeof(args3) / sizeof(args3[0]), &err);
  ASSERT_FALSE(err.HasError()) << err.GetError();
  ASSERT_TRUE(s);
  ASSERT_TRUE((s->fields + 1)->IsNoStem());
  delete s;
}

static void fillSchema(std::vector<char *> &args, size_t nfields) {
  args.resize(1 + nfields * 3);
  args[0] = strdup("SCHEMA");
  size_t n = 1;
  for (unsigned i = 0; i < nfields; i++) {
    asprintf(&args[n++], "field%u", i);
    if (i % 2 == 0) {
      args[n++] = strdup("TEXT");
    } else {
      if (i < 40) {
        // odd fields under 40 are TEXT noINDEX
        args[n++] = strdup("TEXT");
        args[n++] = strdup("NOINDEX");
      } else {
        // the rest are numeric
        args[n++] = strdup("NUMERIC");
      }
    }
  }
  args.resize(n);

  // for (int i = 0; i < n; i++) {
  //   printf("%s ", args[i]);
  // }
  // printf("\n");
}

static void freeSchemaArgs(std::vector<char *> &args) {
  for (auto s : args) {
    free(s);
  }
  args.clear();
}

TEST_F(IndexTest, testHugeSpec) {
  int N = 64;
  std::vector<char *> args;
  fillSchema(args, N);

  QueryError err = {QUERY_OK};
  IndexSpec *s = new IndexSpec("idx", (const char **)&args[0], args.size(), &err);
  ASSERT_FALSE(err.HasError()) << err.GetError();
  ASSERT_TRUE(s);
  ASSERT_TRUE(s->numFields == N);
  delete s;
  freeSchemaArgs(args);

  // test too big a schema
  N = 300;
  fillSchema(args, N);

  err.ClearError();
  s = new IndexSpec("idx", (const char **)&args[0], args.size(), &err);
  ASSERT_TRUE(s == NULL);
  ASSERT_TRUE(err.HasError());
  ASSERT_STREQ("Too many TEXT fields in schema", err.GetError());
  freeSchemaArgs(args);
  err.ClearError();
}

typedef union {

  int i;
  float f;
} u;

TEST_F(IndexTest, testIndexFlags) {

  ForwardIndexEntry h;
  h.docId = 1234;
  h.fieldMask = 0x01;
  h.freq = 1;
  h.vw = new VarintVectorWriter(8);
  for (int n = 0; n < 10; n++) {
    VVW_Write(h.vw, n);
  }
  VVW_Truncate(h.vw);

  uint32_t flags = INDEX_DEFAULT_FLAGS;
  InvertedIndex *w = new InvertedIndex(IndexFlags(flags), 1);
  IndexEncoder enc = InvertedIndex_GetEncoder(w->flags);
  ASSERT_TRUE(w->flags == flags);
  size_t sz = InvertedIndex_WriteForwardIndexEntry(w, enc, &h);
  // printf("written %zd bytes. Offset=%zd\n", sz, h.vw->buf.offset);
  ASSERT_EQ(15, sz);
  InvertedIndex_Free(w);

  flags &= ~Index_StoreTermOffsets;
  w = new InvertedIndex(IndexFlags(flags), 1);
  ASSERT_TRUE(!(w->flags & Index_StoreTermOffsets));
  enc = InvertedIndex_GetEncoder(w->flags);
  size_t sz2 = InvertedIndex_WriteForwardIndexEntry(w, enc, &h);
  // printf("Wrote %zd bytes. Offset=%zd\n", sz2, h.vw->buf.offset);
  ASSERT_EQ(sz2, sz - Buffer_Offset(&h.vw->buf) - 1);
  InvertedIndex_Free(w);

  flags = INDEX_DEFAULT_FLAGS | Index_WideSchema;
  w = new InvertedIndex(IndexFlags(flags), 1);
  ASSERT_TRUE((w->flags & Index_WideSchema));
  enc = InvertedIndex_GetEncoder(w->flags);
  h.fieldMask = 0xffffffffffff;
  ASSERT_EQ(21, InvertedIndex_WriteForwardIndexEntry(w, enc, &h));
  InvertedIndex_Free(w);

  flags |= Index_WideSchema;
  w = new InvertedIndex(IndexFlags(flags), 1);
  ASSERT_TRUE((w->flags & Index_WideSchema));
  enc = InvertedIndex_GetEncoder(w->flags);
  h.fieldMask = 0xffffffffffff;
  sz = InvertedIndex_WriteForwardIndexEntry(w, enc, &h);
  ASSERT_EQ(21, sz);
  InvertedIndex_Free(w);

  flags &= Index_StoreFreqs;
  w = new InvertedIndex(IndexFlags(flags), 1);
  ASSERT_TRUE(!(w->flags & Index_StoreTermOffsets));
  ASSERT_TRUE(!(w->flags & Index_StoreFieldFlags));
  enc = InvertedIndex_GetEncoder(w->flags);
  sz = InvertedIndex_WriteForwardIndexEntry(w, enc, &h);
  ASSERT_EQ(3, sz);
  InvertedIndex_Free(w);

  flags |= Index_StoreFieldFlags | Index_WideSchema;
  w = new InvertedIndex(IndexFlags(flags), 1);
  ASSERT_TRUE((w->flags & Index_WideSchema));
  ASSERT_TRUE((w->flags & Index_StoreFieldFlags));
  enc = InvertedIndex_GetEncoder(w->flags);
  h.fieldMask = 0xffffffffffff;
  sz = InvertedIndex_WriteForwardIndexEntry(w, enc, &h);
  ASSERT_EQ(10, sz);
  InvertedIndex_Free(w);

  VVW_Free(h.vw);
}

TEST_F(IndexTest, testDocTable) {
  char buf[16];
  DocTable dt = new DocTable(10, 10);
  t_docId did = 0;
  // N is set to 100 and the max cap of the doc table is 10 so we surely will
  // get overflow and check that everything works correctly
  int N = 100;
  for (int i = 0; i < N; i++) {
    size_t nkey = sprintf(buf, "doc_%d", i);
    t_docId nd = &dt->Put(buf, nkey, (double)i, Document_DefaultFlags, buf, strlen(buf));
    ASSERT_EQ(did + 1, nd);
    did = nd;
  }

  ASSERT_EQ(N + 1, dt.size);
  ASSERT_EQ(N, dt.maxDocId);
#ifdef __x86_64__
  ASSERT_EQ(10980, (int)dt.memsize);
#endif
  for (int i = 0; i < N; i++) {
    sprintf(buf, "doc_%d", i);
    const char *key = &dt->GetKey(i + 1, NULL);
    ASSERT_STREQ(key, buf);

    float score = &dt->GetScore(i + 1);
    ASSERT_EQ((int)score, i);

    RSDocumentMetadata *dmd = &dt->Get(i + 1);
    dmd->Incref();
    ASSERT_TRUE(dmd != NULL);
    ASSERT_TRUE(dmd->flags & Document_HasPayload);
    ASSERT_STREQ(dmd->keyPtr, buf);
    char *pl = dmd->payload->data;
    ASSERT_TRUE(!(strncmp(pl, (char *)buf, dmd->payload->len)));

    ASSERT_EQ((int)dmd->score, i);
    ASSERT_EQ((int)dmd->flags, (int)(Document_DefaultFlags | Document_HasPayload));

    t_docId xid = &dt.dim->Get(buf, strlen(buf));

    ASSERT_EQ((int)xid, i + 1);

    int rc = &dt->Delete(dmd->keyPtr, sdslen(dmd->keyPtr));
    ASSERT_EQ(1, rc);
    ASSERT_TRUE((int)(dmd->flags & Document_Deleted));
    dmd->Decref();
    dmd = &dt->Get(i + 1);
    ASSERT_TRUE(!dmd);
  }

  ASSERT_FALSE(&dt.dim->Get("foo bar", strlen("foo bar")));
  ASSERT_FALSE(&dt->Get(N + 2));

  t_docId strDocId = &dt->Put("Hello", 5, 1.0, 0, NULL, 0);
  ASSERT_TRUE(0 != strDocId);

  // Test that binary keys also work here
  static const char binBuf[] = {"Hello\x00World"};
  const size_t binBufLen = 11;
  ASSERT_FALSE(&dt.dim->Get(binBuf, binBufLen));
  t_docId binDocId = &dt->Put(binBuf, binBufLen, 1.0, 0, NULL, 0);
  ASSERT_TRUE(binDocId);
  ASSERT_NE(binDocId, strDocId);
  ASSERT_EQ(binDocId, &dt.dim->Get(binBuf, binBufLen));
  ASSERT_EQ(strDocId, &dt.dim->Get("Hello", 5));
  delete &dt;
}

TEST_F(IndexTest, testSortable) {
  RSSortingTable *tbl;
  tbl->Add("foo", RSValue_String);
  tbl->Add("bar", RSValue_String);
  tbl->Add("baz", RSValue_String);
  ASSERT_EQ(3, tbl->len);

  ASSERT_STREQ("foo", tbl->fields[0].name);
  ASSERT_EQ(RSValue_String, tbl->fields[0].type);
  ASSERT_STREQ("bar", tbl->fields[1].name);
  ASSERT_STREQ("baz", tbl->fields[2].name);
  ASSERT_EQ(0, tbl->GetFieldIdx("foo"));
  ASSERT_EQ(0, tbl->GetFieldIdx("FoO"));
  ASSERT_EQ(1, tbl->GetFieldIdx("bar"));
  ASSERT_EQ(-1, tbl->GetFieldIdx("barbar"));

  RSSortingVector *v = new RSSortingVector(tbl->len);
  ASSERT_EQ(v->len, tbl->len);

  const char *str = "hello";
  const char *masse = "Maße";
  double num = 3.141;
  ASSERT_TRUE(v->values[0]->IsNull());
  v->Put(0, str, RS_SORTABLE_STR);
  ASSERT_EQ(v->values[0]->t, RSValue_String);
  ASSERT_EQ(v->values[0]->strval.stype, RSString_RMAlloc);

  ASSERT_TRUE(v->values[1]->IsNull());
  ASSERT_TRUE(v->values[2]->IsNull());
  v->Put(1, &num, RSValue_Number);
  ASSERT_EQ(v->values[1]->t, RS_SORTABLE_NUM);

  RSSortingVector *v2 = new RSSortingVector(tbl->len);
  v2->Put(0, masse, RS_SORTABLE_STR);

  /// test string unicode lowercase normalization
  ASSERT_STREQ("masse", v2->values[0]->strval.str);

  double s2 = 4.444;
  v2->Put(1, &s2, RS_SORTABLE_NUM);

  RSSortingKey sk = {.index = 0, .ascending = 0};

  QueryError qerr;
  QueryError_Init(&qerr);

  int rc = RSSortingVector::Cmp(v, v2, &sk, &qerr);
  ASSERT_LT(0, rc);
  ASSERT_EQ(QUERY_OK, qerr.code);
  sk.ascending = 1;
  rc = RSSortingVector::Cmp(v, v2, &sk, &qerr);
  ASSERT_GT(0, rc);
  ASSERT_EQ(QUERY_OK, qerr.code);
  rc = RSSortingVector::Cmp(v, v, &sk, &qerr);
  ASSERT_EQ(0, rc);
  ASSERT_EQ(QUERY_OK, qerr.code);

  sk.index = 1;

  rc = RSSortingVector::Cmp(v, v2, &sk, &qerr);
  ASSERT_TRUE(-1 == rc && qerr.code == QUERY_OK);
  sk.ascending = 0;
  rc = RSSortingVector::Cmp(v, v2, &sk, &qerr);
  ASSERT_TRUE(1 == rc && qerr.code == QUERY_OK);
}

TEST_F(IndexTest, testVarintFieldMask) {
  t_fieldMask x = 127;
  size_t expected[] = {1, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14, 15, 16, 17, 19};
  Buffer b = {0};
  Buffer_Init(&b, 1);
  BufferWriter bw(&b);
  for (int i = 0; i < sizeof(t_fieldMask); i++, x |= x << 8) {
    size_t sz = bw.WriteVarintFieldMask(x);
    ASSERT_EQ(expected[i], sz);
    bw.Seek(0);
    BufferReader br(bw.buf);

    t_fieldMask y = ReadVarintFieldMask(&br);

    ASSERT_EQ(y, x);
  }
  Buffer_Free(&b);
}

TEST_F(IndexTest, testDeltaSplits) {
  InvertedIndex *idx = new InvertedIndex((IndexFlags)(INDEX_DEFAULT_FLAGS), 1);
  ForwardIndexEntry ent = {0};
  ent.docId = 1;
  ent.fieldMask = RS_FIELDMASK_ALL;

  IndexEncoder enc = InvertedIndex_GetEncoder(idx->flags);
  InvertedIndex_WriteForwardIndexEntry(idx, enc, &ent);
  ASSERT_EQ(idx->size, 1);

  ent.docId = 200;
  InvertedIndex_WriteForwardIndexEntry(idx, enc, &ent);
  ASSERT_EQ(idx->size, 1);

  ent.docId = 1LLU << 48;
  InvertedIndex_WriteForwardIndexEntry(idx, enc, &ent);
  ASSERT_EQ(idx->size, 2);
  ent.docId++;
  InvertedIndex_WriteForwardIndexEntry(idx, enc, &ent);
  ASSERT_EQ(idx->size, 2);

  IndexReader *ir = new TermIndexReader(idx, NULL, RS_FIELDMASK_ALL, NULL, 1);
  IndexResult *h = NULL;
  ASSERT_EQ(INDEXREAD_OK, IR_Read(ir, &h));
  ASSERT_EQ(1, h->docId);

  ASSERT_EQ(INDEXREAD_OK, IR_Read(ir, &h));
  ASSERT_EQ(200, h->docId);

  ASSERT_EQ(INDEXREAD_OK, IR_Read(ir, &h));
  ASSERT_EQ((1LLU << 48), h->docId);

  ASSERT_EQ(INDEXREAD_OK, IR_Read(ir, &h));
  ASSERT_EQ((1LLU << 48) + 1, h->docId);

  ASSERT_EQ(INDEXREAD_EOF, IR_Read(ir, &h));

  IR_Free(ir);
  InvertedIndex_Free(idx);
}