﻿#ifndef _EVALUATE_LEARN_CPP_
#define _EVALUATE_LEARN_CPP_

// KPPT評価関数の学習時用のコード

#include "../shogi.h"

#if defined(EVAL_LEARN)

#include "learn.h"

#include "../evaluate.h"
#include "../eval/evaluate_kppt.h"
#include "../eval/kppt_evalsum.h"
#include "../position.h"
#include "../misc.h"

using namespace std;

namespace Eval
{
	// 絶対値を抑制するマクロ
#define SET_A_LIMIT_TO(X,MIN,MAX)    \
	X[0] = std::min(X[0],(MAX));     \
	X[0] = std::max(X[0],(MIN));     \
	X[1] = std::min(X[1],(MAX));     \
	X[1] = std::max(X[1],(MIN));

	typedef std::array<int32_t, 2> ValueKk;
	typedef std::array<int16_t, 2> ValueKpp;
	typedef std::array<int32_t, 2> ValueKkp;

	// あるBonaPieceを相手側から見たときの値
	BonaPiece inv_piece[fe_end];

	// 盤面上のあるBonaPieceをミラーした位置にあるものを返す。
	BonaPiece mir_piece[fe_end];

	// 学習時にkppテーブルに値を書き出すためのヘルパ関数。
	// この関数を用いると、ミラー関係にある箇所などにも同じ値を書き込んでくれる。次元下げの一種。
	void kpp_write(Square k1, BonaPiece p1, BonaPiece p2, ValueKpp value)
	{
		// '~'をinv記号,'M'をmirror記号だとして
		//   [  k1 ][  p1 ][  p2 ]
		//   [  k1 ][  p2 ][  p1 ]
		//   [M(k1)][M(p1)][M(p2)]
		//   [M(k1)][M(p2)][M(p1)]
		// は、同じ値であるべきなので、その4箇所に書き出す。

		BonaPiece mp1 = mir_piece[p1];
		BonaPiece mp2 = mir_piece[p2];
		Square  mk1 = Mir(k1);

		// kppのppを入れ替えたときのassert

#if 0
		// Apery(WCSC26)の評価関数、玉が5筋にいるきにmirrorした値が一致しないので
		// assertから除外しておく。
		ASSERT_LV3(kpp[k1][p1][p2] == kpp[mk1][mp1][mp2] || file_of(k1) == FILE_5);
		ASSERT_LV3(kpp[k1][p1][p2] == kpp[mk1][mp2][mp1] || file_of(k1) == FILE_5);
#endif

		ASSERT_LV3(kpp[k1][p1][p2] == kpp[k1][p2][p1]);
		kpp[k1][p1][p2]
			= kpp[k1][p2][p1]

#ifdef USE_KPP_MIRROR_WRITE
			// ミラー、書き出すのやめるほうがいいかも。
			= kpp[mk1][mp1][mp2]
			= kpp[mk1][mp2][mp1]
#endif
			= value;
	}

	// kpp_write()するときの一番若いアドレス(index)を返す。
	u64 get_kpp_index(Square k1, BonaPiece p1, BonaPiece p2)
	{
		BonaPiece mp1 = mir_piece[p1];
		BonaPiece mp2 = mir_piece[p2];
		Square  mk1 = Mir(k1);

		const auto q0 = &kpp[0][0][0];
		auto q1 = &kpp[k1][p1][p2] - q0;
		auto q2 = &kpp[k1][p2][p1] - q0;
		auto q3 = &kpp[mk1][mp1][mp2] - q0;
		auto q4 = &kpp[mk1][mp2][mp1] - q0;

		return std::min({ q1, q2, q3, q4 });
	}

	// 学習時にkkpテーブルに値を書き出すためのヘルパ関数。
	// この関数を用いると、ミラー関係にある箇所などにも同じ値を書き込んでくれる。次元下げの一種。
	void kkp_write(Square k1, Square k2, BonaPiece p1, ValueKkp value)
	{
		// '~'をinv記号,'M'をmirror記号だとして
		//   [  k1 ][  k2 ][  p1 ]
		//   [ ~k2 ][ ~k1 ][ ~p1 ] (1)
		//   [M k1 ][M k2 ][M p1 ]
		//   [M~k2 ][M~k1 ][M~p1 ] (2)
		// は、同じ値であるべきなので、その4箇所に書き出す。
		// ただし、(1)[0],(2)[0]は[k1][k2][p1][0]とは符号が逆なので注意。

		BonaPiece ip1 = inv_piece[p1];
		BonaPiece mp1 = mir_piece[p1];
		BonaPiece mip1 = mir_piece[ip1];
		Square  mk1 = Mir(k1);
		Square  ik1 = Inv(k1);
		Square mik1 = Mir(ik1);
		Square  mk2 = Mir(k2);
		Square  ik2 = Inv(k2);
		Square mik2 = Mir(ik2);

		ASSERT_LV3(kkp[k1][k2][p1] == kkp[mk1][mk2][mp1]);

		kkp[k1][k2][p1]
			= kkp[mk1][mk2][mp1]
			= value;

	// kkpに関して180度のflipは入れないほうが良いのでは..
#if USE_KKP_FLIP_WRITE
		ASSERT_LV3(kkp[k1][k2][p1][0] == -kkp[ik2][ik1][ip1][0]);
		ASSERT_LV3(kkp[k1][k2][p1][1] == +kkp[ik2][ik1][ip1][1]);
		ASSERT_LV3(kkp[ik2][ik1][ip1] == kkp[mik2][mik1][mip1]);

		kkp[ik2][ik1][ip1][0]
			= kkp[mik2][mik1][mip1][0]
			= -value[0];

		kkp[ik2][ik1][ip1][1]
			= kkp[mik2][mik1][mip1][1]
			= +value[1];
#endif
	}

#if 0
	// kkp_write()するときの一番若いアドレス(index)を返す。
	u64 get_kkp_index(Square k1, Square k2 , BonaPiece p1)
	{
		BonaPiece ip1 = inv_piece[p1];
		BonaPiece mp1 = mir_piece[p1];
		BonaPiece mip1 = mir_piece[ip1];
		Square  mk1 = Mir(k1);
		Square  ik1 = Inv(k1);
		Square mik1 = Mir(ik1);
		Square  mk2 = Mir(k2);
		Square  ik2 = Inv(k2);
		Square mik2 = Mir(ik2);

		const auto q0 = &kkp[0][0][0];
		auto q1 = &kkp[k1][k2][p1] - q0;
		auto q2 = &kkp[mk1][mk2][mp1] - q0;

		return std::min({ q1, q2 });
	}
#endif

	typedef std::array<LearnFloatType, 2> FloatPair;

	// 出力用。
	std::ostream& operator << (std::ostream& os, const FloatPair& p)
	{
		os << "{ " << p[0] << " , " << p[1] << " } ";
		return os;
	}

	// 勾配等の配列
	struct Weight
	{
		// 元の重み
		FloatPair w;
		
		// mini-batch 1回分の勾配
		FloatPair g;


#if defined (USE_SGD_UPDATE) || defined (USE_YANE_SGD_UPDATE)
		// SGDの更新式
		//   w = w - ηg

		// SGDの場合、勾配自動調整ではないので、損失関数に合わせて適宜調整する必要がある。

		static LearnFloatType eta;

		// この特徴の出現回数
		u32   count;

#endif

#if defined (USE_ADA_GRAD_UPDATE)
		// AdaGradの更新式
		//   v = v + g^2
		// ※　vベクトルの各要素に対して、gの各要素の2乗を加算するの意味
		//   w = w - ηg/sqrt(v)

		// 学習率η = 0.01として勾配が一定な場合、1万回でη×199ぐらい。
		// cf. [AdaGradのすすめ](http://qiita.com/ak11/items/7f63a1198c345a138150)
		// 初回更新量はeta。そこから小さくなっていく。
		static LearnFloatType eta;
		
		// AdaGradのg2
		FloatPair g2;
#endif


#if defined (USE_ADAM_UPDATE)
		// 普通のAdam
		// cf. http://qiita.com/skitaoka/items/e6afbe238cd69c899b2a

//		const LearnFloatType alpha = 0.001f;

		// const double eta = 32.0/64.0;
		// と書くとなぜかeta == 0。コンパイラ最適化のバグか？defineで書く。
		// etaは学習率。FV_SCALE / 64

		static constexpr double beta = LearnFloatType(0.9);
		static constexpr double gamma = LearnFloatType(0.999);
		
		static constexpr double epsilon = LearnFloatType(10e-8);

#if !defined (LEARN_UPDATE_EVERYTIME)
		static constexpr double eta = LearnFloatType(32.0/64.0);
//		static constexpr LearnFloatType  eta = LearnFloatType(1.0);
#else
		// mini-batch size 1Mに対して…。
		// 0.03でも発散気味。よくわからない。
		static constexpr double eta = LearnFloatType(0.01 / 64.0);
#endif


		FloatPair v;
		FloatPair r;

		// これはupdate()呼び出し前に計算して代入されるものとする。
		// bt = pow(β,epoch) , rt = pow(γ,epoch)
		static double bt;
		static double rt;

#endif

#if defined (USE_YANENZA_UPDATE)
		// やねんざメソッドには学習率はない。1か2ずつ適当に動かす。
		u64 count;
#endif

		// 手番用の学習率。これはηより小さめで良いはず。(小さめの値がつくべきところなので)
		// これconstにするとetaに応じてeta2が変わらない。注意すること。
		#define	eta2 (eta / 4)

		void add_grad(LearnFloatType delta1, LearnFloatType delta2)
		{
			g[0] += delta1;
			g[1] += delta2;
#if defined (USE_SGD_UPDATE) || defined(USE_YANE_SGD_UPDATE) || defined(USE_YANENZA_UPDATE)
			count++;
#endif
		}

		// 勾配gにもとづいて、wをupdateする。
		// update後、gは0になる。
		// wをupdateしたならtrueを返す。
		bool update()
		{
#if defined (USE_SGD_UPDATE) || defined(USE_YANE_SGD_UPDATE)

			if (g[0] == 0 && g[1] == 0)
				return false;

			// 勾配はこの特徴の出現したサンプル数で割ったもの。
			if (count == 0)
				goto FINISH;

#if defined(USE_YANE_SGD_UPDATE)

#if 0
			// ゼロ方向に少し引っ張る(L1正則化的な何か)
			w[0] -= (w[0]>0 ? 1 : -1)*0.01f;
			w[1] -= (w[1]>0 ? 1 : -1)*0.01f;
#endif

#if 0
			// 出現頻度が低い特徴は勾配がでたらめである可能性があるのでこれでupdateするのはやめる。
			if (count < 5000)
			{
				count = 0;
				goto FINISH;
			}
#endif
		
#endif

			// 今回の更新量
//			g = { eta * g[0] / count, eta2 * g[1] / count };

			// → eta2では小さすぎのようだ。
			g = { eta * g[0] / count, eta * g[1] / count };

			// あまり大きいと発散しかねないので移動量に制約を課す。
			SET_A_LIMIT_TO(g, -64.0f, 64.0f);

			w = FloatPair{ w[0] - g[0] , w[1] -g[1] };

			count = 0;

//#if defined(USE_YANE_SGD_UPDATE)
//			// ゼロ方向に少し引っ張る(L1正則化的な何か)
//			w[0] -= (w[0]>0 ? 1 : -1)*0.1f;
//			w[1] -= (w[1]>0 ? 1 : -1)*0.1f;
//#endif

#endif

#ifdef USE_ADA_GRAD_UPDATE

			// 普通のAdaGrad

			// g2[i] += g * g;
			// w[i] -= η * g / sqrt(g2[i]);
			// g = 0 // mini-batchならこのタイミングで勾配配列クリア。

			// ゼロ除算を避けるため、abs(g)が小さいときはskipしたほうが良い。
			if (g[0] != 0)
			{
				g2[0] += g[0] * g[0];
				w[0] -= eta * g[0] / sqrt(g2[0]);
			}

			if (g[1] != 0)
			{
				g2[1] += g[1] * g[1];
				w[1] -= eta2 * g[1] / sqrt(g2[1]);
			}
#endif

#ifdef USE_ADAM_UPDATE
			// Adamのときは勾配がゼロのときでもwの更新は行なう。
			//if (g[0] == 0 && g[1] == 0)
			//	return false;

			// v = βv + (1-β)g
			// r = γr + (1-γ)g^2
			// w = w - α*v / (sqrt(r/(1-γ^t))+e) * (1-β^t)
			// rt = 1-γ^t , bt = 1-β^tとして、これは事前に計算しておくとすると、
			// w = w - α*v / (sqrt(r/rt) + e) * (bt)

			v = FloatPair{ float( beta  * v[0] + (1.0 - beta) *g[0]       ) , float(beta  * v[1] + (1.0 - beta) * g[1]       ) };
			r = FloatPair{ float( gamma * r[0] + (1.0 - gamma)*g[0] * g[0]) , float(gamma * r[1] + (1.0 - gamma)* g[1] * g[1]) };

			// sqrt()の中身がゼロになりうるので、1回目の割り算を先にしないとアンダーフローしてしまう。
			// 例) epsilon * bt = 0
			// あと、doubleでないと計算精度が足りなくて死亡する。
			w = FloatPair{ w[0] - LearnFloatType( eta  / (sqrt((double)r[0] / rt) + epsilon) * v[0] / bt) ,
							w[1] - LearnFloatType( eta  / (sqrt((double)r[1] / rt) + epsilon) * v[1] / bt)
			};


#endif

#if defined(USE_YANENZA_UPDATE)


#if 0
			if (g[0] == 0 && g[1] == 0)
				return false;

			if (count < LEARN_MINI_BATCH_SIZE / 40)
			{
				count = 0;

				// これが累積されてくると出現回数が少ないのにwを更新してしまうことになる。
				g = { 0,0 };
				
				goto FINISH;
			}

#else
			// L1正則化的な何かを施す場合
			if ((g[0] == 0 && g[1] == 0)
//				|| (count < LEARN_MINI_BATCH_SIZE / 40)
				// 出現回数が少なすぎる特徴は勾配の方向がでたらめなので動かさない。(過学習の防止)
				)
			{
				count = 0;

				// L1正則化的な何か。
				w = { w[0] * 0.99999f , w[1] * 0.99999f };

				goto FINISH;
			}
#endif

			{
				auto f_sgn = [](float f)
				{
					// fの符号に応じて
					// -3..0..3を二項分布で返す。
					// →　あんまり良くないかも。
					static PRNG prng;
					if (f > 0.1f)
//						return (float)(POPCNT32((u32)prng.rand(4)) + 1);
						return 0.1f;
					if (f < -0.1f)
//						return (float)-(POPCNT32((u32)prng.rand(4)) + 1);
						return -0.1f;

					return 0.0f;
				};

				w = FloatPair{ w[0] - f_sgn(g[0]) , w[1] - f_sgn(g[1]) };
			}

			count = 0;

#endif

		FINISH:;
			g = { 0,0 };

			return true;
		}
	};


#if defined (USE_SGD_UPDATE) || defined(USE_YANE_SGD_UPDATE) || defined(USE_ADA_GRAD_UPDATE)
	LearnFloatType Weight::eta;
#elif defined USE_ADAM_UPDATE
	// 1.0 - pow(beta,epoch)
	double Weight::bt;
	// 1.0 - pow(gamma,epoch)
	double Weight::rt;
#endif

	Weight(*kk_w_)[SQ_NB][SQ_NB];
	Weight(*kpp_w_)[SQ_NB][fe_end][fe_end];
	Weight(*kkp_w_)[SQ_NB][SQ_NB][fe_end];

#define kk_w (*kk_w_)
#define kpp_w (*kpp_w_)
#define kkp_w (*kkp_w_)

	// 学習のときの勾配配列の初期化
	void init_grad()
	{
		if (kk_w_ == nullptr)
		{
			u64 size;

			size = u64(SQ_NB)*u64(SQ_NB);
			kk_w_ = (Weight(*)[SQ_NB][SQ_NB])new Weight[size];
			memset(kk_w_, 0, sizeof(Weight) * size);
#ifdef RESET_TO_ZERO_VECTOR
			cout << "[RESET_TO_ZERO_VECTOR]";
			memset(kk_, 0, sizeof(ValueKk) * size);
#endif

			size = u64(SQ_NB)*u64(fe_end)*u64(fe_end);
			kpp_w_ = (Weight(*)[SQ_NB][fe_end][fe_end])new Weight[size];
			memset(kpp_w_, 0, sizeof(Weight) * size);
#ifdef RESET_TO_ZERO_VECTOR
			memset(kpp_, 0, sizeof(ValueKpp) * size);
#endif

			size = u64(SQ_NB)*u64(SQ_NB)*u64(fe_end);
			kkp_w_ = (Weight(*)[SQ_NB][SQ_NB][fe_end])new Weight[size];
			memset(kkp_w_, 0, sizeof(Weight) * size);
#ifdef RESET_TO_ZERO_VECTOR
			memset(kkp_, 0, sizeof(ValueKkp) * size);
#endif

			// 重みのコピー
			for (auto k1 : SQ)
				for (auto k2 : SQ)
					kk_w[k1][k2].w = { LearnFloatType(kk[k1][k2][0]) , LearnFloatType(kk[k1][k2][1]) };

			for (auto k : SQ)
				for (auto p1 = BONA_PIECE_ZERO; p1 < fe_end; ++p1)
					for (auto p2 = BONA_PIECE_ZERO; p2 < fe_end; ++p2)
						kpp_w[k][p1][p2].w = { LearnFloatType(kpp[k][p1][p2][0]) , LearnFloatType(kpp[k][p1][p2][1]) };

			for (auto k1 : SQ)
				for (auto k2 : SQ)
					for (auto p = BONA_PIECE_ZERO; p < fe_end; ++p)
						kkp_w[k1][k2][p].w = { LearnFloatType(kkp[k1][k2][p][0]) , LearnFloatType(kkp[k1][k2][p][1]) };

		}
	}

	// 現在の局面で出現している特徴すべてに対して、勾配値を勾配配列に加算する。
	void add_grad(Position& pos, Color rootColor, double delta_grad)
	{
		// あまりきつい勾配を持っているとめったに出現しない特徴因子がおかしくなるので制約を課す
		delta_grad = max(delta_grad, -100.0);
		delta_grad = min(delta_grad,  100.0);

		// 勾配配列を確保するメモリがもったいないのでとりあえずfloatでいいや。

		// 手番を考慮しない値
		auto f = (rootColor == BLACK) ? LearnFloatType(delta_grad) : -LearnFloatType(delta_grad);

		// 手番を考慮する値
		auto g = (rootColor == pos.side_to_move()) ? LearnFloatType(delta_grad) : -LearnFloatType(delta_grad);

		Square sq_bk = pos.king_square(BLACK);
		Square sq_wk = pos.king_square(WHITE);
		const auto* ppkppb = kpp[sq_bk];
		const auto* ppkppw = kpp[Inv(sq_wk)];

		auto& pos_ = *const_cast<Position*>(&pos);

		auto list_fb = pos_.eval_list()->piece_list_fb();
		auto list_fw = pos_.eval_list()->piece_list_fw();

		int i, j;
		BonaPiece k0, k1, l0, l1;

		// KK
		kk_w[sq_bk][sq_wk].add_grad( f , g );

#ifdef LEARN_UPDATE_EVERYTIME
		kk_w[sq_bk][sq_wk].update();
#endif

		for (i = 0; i < PIECE_NO_KING; ++i)
		{
			k0 = list_fb[i];
			k1 = list_fw[i];
			for (j = 0; j < i; ++j)
			{
				l0 = list_fb[j];
				l1 = list_fw[j];

				// KPP

				// kpp配列に関してはミラー(左右判定)とフリップ(180度回転)の次元下げを行う。

				// kpp_w[sq_bk][k0][l0].add_grad(ValueKppFloat{ f ,  g });
				// kpp_w[Inv(sq_wk)][k1][l1].add_grad(ValueKppFloat{ -f ,  g });

#if 1

#if !defined( LEARN_UPDATE_EVERYTIME)
				// KPPの手番ありのとき
				((Weight*)kpp_w_)[get_kpp_index(sq_bk, k0, l0)].add_grad( f ,  g );
				((Weight*)kpp_w_)[get_kpp_index(Inv(sq_wk), k1, l1)].add_grad( -f ,  g );
#else
				kpp_w[sq_bk][k0][l0].add_grad(f, g);
				kpp_w[sq_bk][l0][k0].add_grad(f, g);
				kpp_w[Inv(sq_wk)][k1][l1].add_grad(-f, g);
				kpp_w[Inv(sq_wk)][l1][k1].add_grad(-f, g);

				kpp_w[sq_bk][k0][l0].update();
				kpp_w[sq_bk][l0][k0].update();
				kpp_w[Inv(sq_wk)][k1][l1].update();
				kpp_w[Inv(sq_wk)][l1][k1].update();
#endif

#else
				// KPPの手番はなしのとき
				((Weight*)kpp_w_)[get_kpp_index(sq_bk, k0, l0)].add_grad(f, 0);
				((Weight*)kpp_w_)[get_kpp_index(Inv(sq_wk), k1, l1)].add_grad(-f, 0);
#endif

#if 0
				// ミラーもやめると…？
				kpp_w[sq_bk][k0][l0].add_grad( f ,  g );
				kpp_w[sq_bk][l0][k0].add_grad( f ,  g );
				kpp_w[Inv(sq_wk)][k1][l1].add_grad( -f ,  g );
				kpp_w[Inv(sq_wk)][l1][k1].add_grad( -f ,  g );
#endif
			}

			// ((Weight*)kkp_w_)[get_kkp_index(sq_bk,sq_wk,k0)].add_grad( f , g );

			// kkpは次元下げ、ミラーも含めてやらないことにする。どうせ教師の数は足りているし、
			// 右と左とでは居飛車、振り飛車的な戦型選択を暗に含むからミラーするのが良いとは限らない。
			// 180度回転も先手後手とは非対称である可能性があるのでここのフリップも入れない。

			// KKP

			kkp_w[sq_bk][sq_wk][k0].add_grad( f , g );

#ifdef LEARN_UPDATE_EVERYTIME
			kkp_w[sq_bk][sq_wk][k0].update();
#endif

		}

	}


	// 現在の勾配をもとにSGDかAdaGradか何かする。
	void update_weights(u64 mini_batch_size , u64 epoch)
	{
		// kppの最小値、最大値、絶対値の和を表示させることで学習が進んでいるかのチェックに用いる。
#ifdef DISPLAY_STATS_IN_UPDATE_WEIGHTS
		FloatPair min_kpp = { 0.0f , 0.0f };
		FloatPair max_kpp = { 0.0f , 0.0f };
		FloatPair sum_kpp = { 0.0f , 0.0f };
#endif

		//
		// 学習メソッドに応じた学習率設定
		//

		// SGD
#if defined (USE_SGD_UPDATE) || defined(USE_YANE_SGD_UPDATE)

#if defined (LOSS_FUNCTION_IS_CROSS_ENTOROPY)

#ifdef USE_SGD_UPDATE
		Weight::eta = 3.2f;

		//Weight::eta = 100.0f;
#endif

#ifdef USE_YANE_SGD_UPDATE
		//		Weight::eta = 100.0f;

		// epoch == 100(1億局面)で0.9倍。10億で0.3倍みたいな。

		// あとη、大きめに。
		Weight::eta = 32.0f * (float)pow(0.999f, epoch);

		// 1億で0.8倍、10億で0.13倍。
//		Weight::eta = 100.0f * (float)pow(0.998f, epoch);

		// 1億で0.74倍、10億で0.05倍。
//		Weight::eta = 300.0f * (float)pow(0.997f, epoch);


#endif

#elif defined (LOSS_FUNCTION_IS_WINNING_PERCENTAGE)

#ifdef USE_SGD_UPDATE
//		Weight::eta = 150.0f;

		Weight::eta = 32.0f;
#endif

#ifdef USE_YANE_SGD_UPDATE
		Weight::eta = 100.0f * (float)pow(0.999f, epoch);
#endif
#endif

		// AdaGrad
#elif defined USE_ADA_GRAD_UPDATE

#ifdef LEARN_UPDATE_EVERYTIME
		// mini-batch size = 1Mに対して0.2f～0.4fぐらいが適切
		Weight::eta = 0.5f;

#else
		// この係数、mini-batch sizeの影響を受けるのどうかと思うが..
		Weight::eta = 3.0f;
#endif

		// Adam
#elif defined USE_ADAM_UPDATE

		Weight::bt = 1.0 - pow((double)Weight::beta , (double)epoch);
		Weight::rt = 1.0 - pow((double)Weight::gamma, (double)epoch);

#endif

#ifdef DISPLAY_STATS_IN_UPDATE_WEIGHTS
		int max_kpp_k = -1, max_kpp_p1 = -1, max_kpp_p2 = -1;
#endif

// 学習をopenmpで並列化(この間も局面生成は続くがまあ、問題ないやろ..
#ifdef _OPENMP
#pragma omp parallel
#endif
		{
#ifdef _OPENMP
#pragma omp for
#endif
			// Open MP対応のため、int型の変数を使う必要がある。(悲しい)
			for (int k1 = SQ_ZERO; k1 < SQ_NB; ++k1)
			{
				for (auto k2 : SQ)
				{
					auto& w = kk_w[k1][k2];

					// wの値にupdateがあったなら、値を制限して、かつ、kkに反映させる。
#if ! defined(LEARN_UPDATE_EVERYTIME)
					if (w.update())
#endif
					{
						// 絶対値を抑制する。
						SET_A_LIMIT_TO(w.w, LearnFloatType((s32)INT16_MIN * 4), LearnFloatType((s32)INT16_MAX * 4));
						kk[k1][k2] = { (s32)w.w[0], (s32)w.w[1] };
					}
				}
			}

#ifdef _OPENMP
#pragma omp for
#endif
			for (int p1 = BONA_PIECE_ZERO; p1 < fe_end; ++p1)
			{
				for (auto k : SQ)
					for (auto p2 = BONA_PIECE_ZERO; p2 < fe_end; ++p2)
					{
						// p1とp2を入れ替えたものは、kpp_write()で書き込まれるはずなので無視して良い。
						if (p1 > p2)
							continue;

						auto& w = kpp_w[k][p1][p2];

						// p1,p2を入れ替えたものも集計する。
						auto& w2 = kpp_w[k][p2][p1];
						w.g += w2.g;
						w2.g = { 0.0f , 0.0f };

#if defined (USE_SGD_UPDATE) || defined (USE_YANE_SGD_UPDATE) || defined(USE_YANENZA_UPDATE)
						w.count += w2.count;
						w2.count = 0;
#endif

#if defined (USE_ADA_GRAD_UPDATE) && defined(LEARN_UPDATE_EVERYTIME)
						// g2を指数移動平均で減衰させておかないと値が動かなくなって良くないような..
						w.g2[0] *= 0.999f;
						w.g2[1] *= 0.999f;
						w2.g2[0] *= 0.999f;
						w2.g2[1] *= 0.999f;
#endif
						

#ifdef DISPLAY_STATS_IN_UPDATE_WEIGHTS
						min_kpp = { min(min_kpp[0], w.w[0]) , min(min_kpp[1], w.w[1]) };
						max_kpp = { max(max_kpp[0], w.w[0]) , max(max_kpp[1], w.w[1]) };
						sum_kpp = { sum_kpp[0] + abs(w.w[0]) , sum_kpp[1] + abs(w.w[1]) };

						// デバッグ用に、これがどこの値であるかを表示する。
						if (max_kpp[0] == w.w[0])
						{
							max_kpp_k = k;
							max_kpp_p1 = p1;
							max_kpp_p2 = p2;
						}

#endif

#if ! defined(LEARN_UPDATE_EVERYTIME)
						if (w.update())
#endif
						{
							// 絶対値を抑制する。
							SET_A_LIMIT_TO(w.w, (LearnFloatType)(INT16_MIN / 2), (LearnFloatType)(INT16_MAX / 2));

							kpp_write(k, (BonaPiece)p1, p2, ValueKpp{ (s16)w.w[0], (s16)w.w[1] });

							//kpp[k][p1][p2] = ValueKpp{ (s16)w.w[0], (s16)w.w[1] };

							//kpp[k][p1][p2] = ValueKpp{ (s16)w.w[0], (s16)w.w[1] };
							//kpp[k][p2][p1] = ValueKpp{ (s16)w.w[0], (s16)w.w[1] };
						}
					}
			}

			// 外側のループをk1にすると、ループ回数が81になって、40HTのときに1余るのが嫌。
			// ゆえに外側のループはpに変更する。
#ifdef _OPENMP
#pragma omp for
#endif
			for (int p = BONA_PIECE_ZERO; p < fe_end; ++p)
			{
				for (auto k1 : SQ)
					for (auto k2 : SQ)
					{
						auto& w = kkp_w[k1][k2][p];

						// cout << "\n" << w.g[0] << " & " << w.g[1];

#if ! defined(LEARN_UPDATE_EVERYTIME)
						if (w.update())
#endif
						{
							// 絶対値を抑制する。
							SET_A_LIMIT_TO(w.w, (LearnFloatType)(INT16_MIN / 2), (LearnFloatType)(INT16_MAX / 2));

							//	kkp_write(k1, k2, p, ValueKkp{s32(w.w[0]),s32(w.w[1])});

							// kkpは上で書いた理由で次元下げをしない。
							kkp[k1][k2][p] = ValueKkp{ s32(w.w[0]),s32(w.w[1]) };
						}
					}
			}
		}

#ifdef DISPLAY_STATS_IN_UPDATE_WEIGHTS
		cout << "\n min_kpp = " << min_kpp << " , max_kpp = " << max_kpp << " , sum_kpp = " << sum_kpp << " ";
		cout << "\n max_kpp (k,p1,p2) = " << "(" << max_kpp_k << " , " << max_kpp_p1 << " , " << max_kpp_p2 << ")";
#endif
	}


	// 学習のためのテーブルの初期化
	void eval_learn_init()
	{
		// fとeとの交換
		int t[] = {
			f_hand_pawn - 1    , e_hand_pawn - 1   ,
			f_hand_lance - 1   , e_hand_lance - 1  ,
			f_hand_knight - 1  , e_hand_knight - 1 ,
			f_hand_silver - 1  , e_hand_silver - 1 ,
			f_hand_gold - 1    , e_hand_gold - 1   ,
			f_hand_bishop - 1  , e_hand_bishop - 1 ,
			f_hand_rook - 1    , e_hand_rook - 1   ,
			f_pawn             , e_pawn            ,
			f_lance            , e_lance           ,
			f_knight           , e_knight          ,
			f_silver           , e_silver          ,
			f_gold             , e_gold            ,
			f_bishop           , e_bishop          ,
			f_horse            , e_horse           ,
			f_rook             , e_rook            ,
			f_dragon           , e_dragon          ,
		};

		// 未初期化の値を突っ込んでおく。
		for (BonaPiece p = BONA_PIECE_ZERO; p < fe_end; ++p)
		{
			inv_piece[p] = (BonaPiece)-1;

			// mirrorは手駒に対しては機能しない。元の値を返すだけ。
			mir_piece[p] = (p < f_pawn) ? p : (BonaPiece)-1;
		}

		for (BonaPiece p = BONA_PIECE_ZERO; p < fe_end; ++p)
		{
			for (int i = 0; i < 32 /* t.size() */; i += 2)
			{
				if (t[i] <= p && p < t[i + 1])
				{
					Square sq = (Square)(p - t[i]);

					// 見つかった!!
					BonaPiece q = (p < fe_hand_end) ? BonaPiece(sq + t[i + 1]) : (BonaPiece)(Inv(sq) + t[i + 1]);
					inv_piece[p] = q;
					inv_piece[q] = p;

					/*
					ちょっとトリッキーだが、pに関して盤上の駒は
					p >= fe_hand_end
					のとき。

					このpに対して、nを整数として(上のコードのiは偶数しかとらない)、
					a)  t[2n + 0] <= p < t[2n + 1] のときは先手の駒
					b)  t[2n + 1] <= p < t[2n + 2] のときは後手の駒
					　である。

					 ゆえに、a)の範囲にあるpをq = Inv(p-t[2n+0]) + t[2n+1] とすると180度回転させた升にある後手の駒となる。
					 そこでpとqをswapさせてinv_piece[ ]を初期化してある。
					 */

					 // 手駒に関してはmirrorなど存在しない。
					if (p < fe_hand_end)
						continue;

					BonaPiece r1 = (BonaPiece)(Mir(sq) + t[i]);
					mir_piece[p] = r1;
					mir_piece[r1] = p;

					BonaPiece p2 = (BonaPiece)(sq + t[i + 1]);
					BonaPiece r2 = (BonaPiece)(Mir(sq) + t[i + 1]);
					mir_piece[p2] = r2;
					mir_piece[r2] = p2;

					break;
				}
			}
		}

		for (BonaPiece p = BONA_PIECE_ZERO; p < fe_end; ++p)
			if (inv_piece[p] == (BonaPiece)-1
				|| mir_piece[p] == (BonaPiece)-1
				)
			{
				// 未初期化のままになっている。上のテーブルの初期化コードがおかしい。
				ASSERT(false);
			}

#if 0
		// 評価関数のミラーをしても大丈夫であるかの事前検証
		// 値を書き込んだときにassertionがあるので、ミラーしてダメである場合、
		// そのassertに引っかかるはず。

		// AperyのWCSC26の評価関数、kppのp1==0とかp1==20(後手の0枚目の歩)とかの
		// ところにゴミが入っていて、これを回避しないとassertに引っかかる。

		std::unordered_set<BonaPiece> s;
		vector<int> a = {
			f_hand_pawn - 1,e_hand_pawn - 1,
			f_hand_lance - 1, e_hand_lance - 1,
			f_hand_knight - 1, e_hand_knight - 1,
			f_hand_silver - 1, e_hand_silver - 1,
			f_hand_gold - 1, e_hand_gold - 1,
			f_hand_bishop - 1, e_hand_bishop - 1,
			f_hand_rook - 1, e_hand_rook - 1,
		};
		for (auto b : a)
			s.insert((BonaPiece)b);

		// さらに出現しない升の盤上の歩、香、桂も除外(Aperyはここにもゴミが入っている)
		for (Rank r = RANK_1; r <= RANK_2; ++r)
			for (File f = FILE_1; f <= FILE_9; ++f)
			{
				if (r == RANK_1)
				{
					// 1段目の歩
					BonaPiece b1 = BonaPiece(f_pawn + (f | r));
					s.insert(b1);
					s.insert(inv_piece[b1]);

					// 1段目の香
					BonaPiece b2 = BonaPiece(f_lance + (f | r));
					s.insert(b2);
					s.insert(inv_piece[b2]);
				}

				// 1,2段目の桂
				BonaPiece b = BonaPiece(f_knight + (f | r));
				s.insert(b);
				s.insert(inv_piece[b]);
			}

		cout << "\nchecking kpp_write()..";
		for (auto sq : SQ)
		{
			cout << sq << ' ';
			for (BonaPiece p1 = BONA_PIECE_ZERO; p1 < fe_end; ++p1)
				for (BonaPiece p2 = BONA_PIECE_ZERO; p2 < fe_end; ++p2)
					if (!s.count(p1) && !s.count(p2))
						kpp_write(sq, p1, p2, kpp[sq][p1][p2]);
		}
		cout << "\nchecking kkp_write()..";

		for (auto sq1 : SQ)
		{
			cout << sq1 << ' ';
			for (auto sq2 : SQ)
				for (BonaPiece p1 = BONA_PIECE_ZERO; p1 < fe_end; ++p1)
					if (!s.count(p1))
						kkp_write(sq1, sq2, p1, kkp[sq1][sq2][p1]);
		}
		cout << "..done!" << endl;
#endif
	}


	void save_eval(std::string dir_name)
	{
		{
			auto eval_dir = path_combine((string)Options["EvalSaveDir"], dir_name);

			// すでにこのフォルダがあるならmkdir()に失敗するが、
			// 別にそれは構わない。なければ作って欲しいだけ。
			// また、EvalSaveDirまでのフォルダは掘ってあるものとする。
			
			MKDIR(eval_dir);

			// KK
			std::ofstream ofsKK(path_combine(eval_dir , KK_BIN) , std::ios::binary);
			if (!ofsKK.write(reinterpret_cast<char*>(kk), sizeof(kk)))
				goto Error;

			// KKP
			std::ofstream ofsKKP(path_combine(eval_dir , KKP_BIN) , std::ios::binary);
			if (!ofsKKP.write(reinterpret_cast<char*>(kkp), sizeof(kkp)))
				goto Error;

			// KPP
			std::ofstream ofsKPP(path_combine(eval_dir , KPP_BIN) , std::ios::binary);
			if (!ofsKPP.write(reinterpret_cast<char*>(kpp), sizeof(kpp)))
				goto Error;

			cout << "save_eval() finished. folder = " << eval_dir <<  endl;

			return;
		}

	Error:;
		cout << "Error : save_eval() failed" << endl;
	}

} // namespace Eval

#endif // EVAL_LEARN

#endif
