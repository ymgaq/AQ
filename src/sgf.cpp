#include <algorithm>
#include <assert.h>

#include "sgf.h"

using std::string;
using std::endl;

SgfData::SgfData() {

	Clear();

}

void SgfData::Clear() {

	rule = "Chinese";
	board_size = 19;
	komi = 6.5;
	player[0] = "White";
	player[1] = "Black";
	rating[0] = rating[1] = 2800;
	handicap = 0;
	winner = -1;
	handicap_stone[0].clear();
	handicap_stone[1].clear();
	move_history.clear();
	move_cnt = 0;
	is_black_first = true;
	score = 0.0;

}

void SgfData::AddMove(int v){

	move_history.push_back(v);
	++move_cnt;

}

/**
 *  SGFの着手文字列を座標に変換する
 *  Convert move string of sgf to [0, 441)
 *  "DQ" -> (4,16) -> 340
 *  "qf" -> (16,6) -> 142
 */
int SgfData::ConvertToVertex(string aa) {

	// 入力サイズが2でないときはパスを返す
	// Return PASS if input size is not 2.
	if (aa.size() != 2) return (board_size + 2)*(board_size + 2);

	// aaを(x,y)実座標に変換
	// Convert aa to (x, y) of the real board.
	int x, y;
	if (isupper(aa.substr(0, 1)[0]) != 0) 	{ x = (int)aa.substr(0, 1)[0] - 'A'; }
	else									{ x = (int)aa.substr(0, 1)[0] - 'a'; }
	if (isupper(aa.substr(1, 1)[0]) != 0) 	{ y = (int)aa.substr(1, 1)[0] - 'A'; }
	else									{ y = (int)aa.substr(1, 1)[0] - 'a'; }

	// 盤外のときパスを返す
	// Return PASS when it is on the outer baundary.
	if(x >= board_size || y >= board_size){
		return (board_size + 2)*(board_size + 2);
	}

	// yを反転させて拡張座標に変換
	// Convert to the coordinate of the extended board.
	return (x + 1)+(board_size - y)*(board_size + 2);

}

/**
 *  SGFファイルから対局データを抽出する
 *  Extract data from the SGF file.
 */
void SgfData::ImportData(string file_name) {

	// ファイルを開く. Open the file.
	std::stringstream fn;
	fn << file_name;
	std::ifstream ifs(fn.str());
	string str;

	// 1行ずつ読み出していく
	// Read lines.
	while (ifs && getline(ifs, str)) {

		// 残りが4文字未満のときは次の行に
		// Move to the next line when remaining letters are less than 4.
		while (str.size() > 3) {

			// (;GM[1]FF[4]CA[UTF-8]... のように記述される
			// 2文字のtagと[]内のin_brを順番に読み出す
			// The beginning is typically written as '(;GM[1]FF[4]CA[UTF-8]...'
			// Read two-character tags and contents in [] in order.
			string tag, in_br;

			// []が見つからないとき、次の行に
			// Go to the next line if [] is not found.
			string::size_type open_br = str.find("[");
			string::size_type close_br = str.find("]");

			if (open_br == string::npos || close_br == string::npos) break;

			if(close_br == 0){
				str = str.substr(close_br + 1);
				close_br = str.find("]");
				open_br = str.find("[");
			}

			tag = str.substr(0, open_br);
			in_br = str.substr(open_br + 1, close_br - open_br - 1);

			//std::cerr << "tag: " << tag << " in_br: " << in_br << endl;
			//getchar();

			// 区切り文字のセミコロンをtagから除く
			// Remove semicolon from the tag.
			string::size_type semicolon = tag.find(";");
			if (semicolon != string::npos) tag = tag.substr(semicolon + 1);

			if (tag == "RU"){
				// ルール. Rule.
				// Chinese, Japanese
				rule = in_br;
			}
			else if (tag == "SZ"){
				// 盤面サイズ. Board size.
				// 19
				board_size = stoi(in_br);

				assert(board_size == 19);
			}
			else if (tag == "KM") {
				// コミ. Komi.

				// 逆コミかを調べる. Check whether in_br is negative.
				// [-6.5], ...
				int sign_num = 1;
				if(in_br.substr(0,1).find("-") != string::npos){
					sign_num = -1;
					in_br = in_br.substr(1);
				}

				// in_brが数値であるかを確認
				// Check whether in_br is numeric.
				if (!in_br.empty() && std::find_if(in_br.begin(),
						in_br.end(), [](char c) { return !std::isdigit(c); }) == in_br.end())
				{
					// 中国ルールの場合、コミ7.5目が[3.75]と記述される
					// 2倍して整数になるかを確認
					// Check whether it becomes an integer when doubled.
					double tmp_floor = 2 * stod(in_br) - floor(2 * stod(in_br));
					// 中国式なので2倍する.
					// Double komi for Chinese rule.
					if(tmp_floor != 0) komi = 2 * sign_num * stod(in_br);
					// 日本式. Japanese rule.
					else komi = sign_num * stod(in_br);
				}

			}
			else if (tag == "PW"){
				// 白の対局者名. Player name of white.
				player[0] = in_br;
			}
			else if (tag == "PB"){
				// 黒の対局者名. Player name of black.
				player[1] = in_br;
			}
			else if (tag == "WR" || tag == "BR") {
				// 段位・レーティング. Rating.

				// 末尾の?を除く. Exclude the trailing '?'.
				if(in_br.find("?") != string::npos) in_br = in_br.substr(0, (int)in_br.length() - 1);

				// in_brが数値のとき、生のレーティング
				// Input the rating if in_br is numeric.
				if (!in_br.empty() && std::find_if(in_br.begin(),
					in_br.end(), [](char c) { return !std::isdigit(c); }) == in_br.end()){
					if (tag == "WR")	rating[0] = stoi(in_br);
					else				rating[1] = stoi(in_br);
				}
				// 2文字のとき、段位からレーティングを計算
				// Calculate rating from the rank if the length of in_br is 2.
				// Ex. 9p, 6D, 2k
				else if(in_br.length() == 2){
					string::size_type p = in_br.find("p");
					string::size_type P = in_br.find("P");
					string::size_type d = in_br.find("d");
					string::size_type D = in_br.find("D");
					string::size_type k = in_br.find("k");
					if (p != string::npos || P != string::npos) {
						// プロはR3000. 3000 if a professional player.
						if (tag == "WR")	rating[0] = 3000;
						else				rating[1] = 3000;
					}
					else if (d != string::npos || D != string::npos) {
						// 1d = 1580, 2d = 1760, ... 9d = 3020
						if (tag == "WR")	rating[0] = 1400 + stoi(in_br.substr(0,1)) * 180;
						else				rating[1] = 1400 + stoi(in_br.substr(0,1)) * 180;
					}
					else if (k != string::npos) {
						// 1k = 1450, 2k = 1350, ...
						if (tag == "WR")	rating[0] = 1550 - stoi(in_br.substr(0,1)) * 100;
						else				rating[1] = 1550 - stoi(in_br.substr(0,1)) * 100;
					}
				}

			}
			else if (tag == "HA") {
				// 置石の数. Number of the handicap stones.
				// 2, 3, 4, ...

				// in_brが数値かを確認
				// Check whether in_br is numeric.
				if (!in_br.empty() && std::find_if(in_br.begin(),
					in_br.end(), [](char c) { return !std::isdigit(c); }) == in_br.end()){
					handicap = stoi(in_br);
				}
			}
			else if (tag == "RE") {
				// 対局結果. Result.
				// W+R, B+Resign, W+6.5, B+Time, ...

				string::size_type b = in_br.find("B+");
				string::size_type w = in_br.find("W+");
				if(in_br.length() <= 12){
					if (b != string::npos){
						// 黒n目勝ち. Black wins by n stones.
						if (!in_br.substr(2).empty() && std::find_if(in_br.substr(2).begin(),
								in_br.substr(2).end(), [](char c) { return !std::isdigit(c); }) == in_br.substr(2).end()){
							score = std::stod(in_br.substr(2));
							// 2倍して整数になるかを確認
							if(2 * score - floor(2 * score) != 0) score *= 2;
							winner = 1;
						}
						// 黒中押し勝ち. Black wins by resign.
						else if(in_br.find("R") != string::npos) winner = 1;
					}
					else if (w != string::npos){
						// 白n目勝ち. White wins by n stones.
						if (!in_br.substr(2).empty() && std::find_if(in_br.substr(2).begin(),
								in_br.substr(2).end(), [](char c) { return !std::isdigit(c); }) == in_br.substr(2).end()){
							score = std::stod(in_br.substr(2));
							// 2倍して整数になるかを確認
							if(2 * score - floor(2 * score) != 0) score *= 2;
							winner = 0;
						}
						// 白中押し勝ち. White wins by resign.
						else if(in_br.find("R") != string::npos) winner = 0;
					}
				}
			}
			else if (tag == "B") {
				// 黒の着手. Move of black.

				if (move_cnt == 0) is_black_first = true;
				++move_cnt;
				move_history.push_back(ConvertToVertex(in_br));
			}
			else if (tag == "W") {
				// 白の着手. Move of white.

				// 白から打ち始める棋譜用. For a handicap match.
				if (move_cnt == 0) {
					is_black_first = false;
					++move_cnt;
					// 黒の初手パスを追加. Add PASS of black.
					move_history.push_back((board_size+2)*(board_size + 2));
					komi = 0;
				}
				++move_cnt;
				move_history.push_back(ConvertToVertex(in_br));
			}
			else if (tag == "AB" || tag == "AW") {
				// 置石. Handicap stones.
				// AB[dd][qq], ...

				handicap_stone[int(tag == "AB")].push_back(ConvertToVertex(in_br));
				string::size_type next_open_br = str.substr(close_br + 1).find("[");
				string::size_type next_close_br = str.substr(close_br + 1).find("]");

				// 連続して続く[]を読み込む
				// Read continuous [].
				while (next_open_br == 0) {
					open_br = close_br + 1 + next_open_br;
					close_br = close_br + 1 + next_close_br;

					std::stringstream ss_in_br;
					in_br = "";
					ss_in_br << str.substr(open_br + 1, close_br - open_br - 1);
					ss_in_br >> in_br;
					handicap_stone[int(tag == "AB")].push_back(ConvertToVertex(in_br));

					next_open_br = str.substr(close_br + 1).find("[");
					next_close_br = str.substr(close_br + 1).find("]");
				}
			}

			// strからtag[in_br]までをカット
			// Excludes tag[in_br] from str.
			str = str.substr(close_br + 1);
		}		
	}

}

/**
 *  対局データをSGFファイルに出力する
 *  Output the match information to an SGF file.
 */
void SgfData::ExportData(string file_name){

	// ファイルを開く. Open the file.
	std::ofstream ofs(file_name.c_str());

	// ヘッダは固定. Use the fixed header.
	ofs << "(;GM[1]FF[4]CA[UTF-8]" << endl;
	ofs << "RU[Chinese]SZ[19]KM[7.5]" << endl;

	string str = "abcdefghijklmnopqrs";
	for(int i=0;i<(int)move_history.size();++i){
		int v = move_history[i];
		int x = etox[v] - 1;
		int y = EBSIZE - etoy[v] - 2;

		if(i%2 == 0){
			ofs << ";B[";
		}
		else{
			ofs << ";W[";
		}
		if(v < PASS){
			ofs << str.substr(x, 1) << str.substr(y, 1) << "]";
		}
		else ofs << "]";

		if((i+1) % 8 == 0) ofs << endl;
	}
	ofs << ")" << endl;

	ofs.close();

}

/**
 *  対局データをコメント付きでSGFファイルに出力する
 *  Output the match information to an SGF file with comments.
 */
void SgfData::ExportData(std::string file_name, std::vector<std::string> cmt_list){

	// ファイルを開く. Open the file.
	std::ofstream ofs(file_name.c_str());

	// ヘッダは固定. Use the fixed header.
	ofs << "(;GM[1]FF[4]CA[UTF-8]" << endl;
	ofs << "RU[Chinese]SZ[19]KM[7.5]" << endl;

	string str = "abcdefghijklmnopqrs";
	for(int i=0;i<(int)move_history.size();++i){
		int v = move_history[i];
		int x = etox[v] - 1;
		int y = EBSIZE - etoy[v] - 2;

		if(i%2 == 0){
			ofs << ";B[";
		}
		else{
			ofs << ";W[";
		}
		if(v < PASS){
			ofs << str.substr(x, 1) << str.substr(y, 1) << "]";
		}
		else ofs << "]";

		if((int)cmt_list.size() > i){
			ofs << "C[" << cmt_list[i] << "]";
		}

		if((i+1) % 8 == 0) ofs << endl;
	}
	ofs << ")" << endl;

	ofs.close();

}


/**
 *  SGFファイルから盤面を生成する
 *  Generates a board from the SGF file.
 */
bool SgfData::GenerateBoard(Board& b, int move_idx) {

	if (move_idx > move_cnt) return false;
	b.Clear();

	// 置石を配置. Place handicap stones.
	int handicap_stone_cnt = std::max((int)handicap_stone[0].size(), (int)handicap_stone[1].size());
	for (int i=0;i<handicap_stone_cnt;++i) {
		if ((int)handicap_stone[1].size() > i) {
			if (!b.IsLegal(b.my, handicap_stone[1][i])) return false;
			b.PlayLegal(handicap_stone[1][i]);
		}
		else b.PlayLegal(PASS);
		if ((int)handicap_stone[0].size() > i) {
			if (!b.IsLegal(b.my, handicap_stone[0][i])) return false;
			b.PlayLegal(handicap_stone[0][i]);
		}
		else b.PlayLegal(PASS);
	}

	// 置石は手数に含まない. Reset move_cnt.
	b.move_history.clear();
	b.move_cnt = 0;
	b.pass_cnt[0] = b.pass_cnt[1] = 0;

	// 初期局面. Initial board.
	if (move_idx == 0) return true;

	// n手目まで進める. Play until move_idx.
	for (int i=0;i<move_idx;++i) {
		if (!b.IsLegal(b.my, move_history[i])) return false;
		b.PlayLegal(move_history[i]);
	}

	return true;
}

/**
 *  フォルダ内のSGFファイルを全て読み込む
 *  Read all SGF files in the folder.
 */
#ifdef _WIN32
int ImportSGFList(string folder, std::vector<SgfData>& sgf_list) {
	int sgf_list_cnt = 0;
	std::vector<string> file_list;
	HANDLE h_find;
	WIN32_FIND_DATA fd;

	std::stringstream ss;
	ss << folder;
	string::iterator itr = folder.end();
	itr--;
	if (*itr != '\\') ss << '\\';
	string path = ss.str();
	ss << "*.sgf";
	//wchar_t* wc = new wchar_t[1024];
	size_t converted_char = 0;
	//mbstowcs_s(&converted_char, wc, 1024, ss.str().c_str(), _TRUNCATE);
	h_find = FindFirstFile(ss.str().c_str(), &fd);
	//h_find = FindFirstFile(wc, &fd);

	// 検索失敗
	if (h_find == INVALID_HANDLE_VALUE) {
		return sgf_list_cnt;
	}

	do {
		// フォルダは除く
		if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			&& !(fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN))
		{
			//char *file = new char[1024];
			//wcstombs_s(&converted_char, file, 1024, fd.cFileName, _TRUNCATE);
			//string str = file;
			string str = fd.cFileName;
			string full_path = path + str;

			SgfData tmp_sgf_data;
			tmp_sgf_data.ImportData(full_path);
			sgf_list.push_back(tmp_sgf_data);
			++sgf_list_cnt;
		}
	} while (FindNextFile(h_find, &fd)); //次のファイルを探索

										// hFindのクローズ
	FindClose(h_find);
	return sgf_list_cnt;
}
#else
int ImportSGFList(string folder, std::vector<SgfData>& sgf_list) {

	Board b;
	int sgf_list_cnt = 0;
	DIR *dr = opendir(folder.c_str());
	if (dr == NULL) return sgf_list_cnt;
	dirent* entry;
	do {
		entry = readdir(dr);
		if (entry != NULL) {
			string file_name = entry->d_name;
			if (file_name.find(".sgf") == string::npos) continue;
			string full_path = folder + file_name;
			SgfData tmp_sgf_data;
			tmp_sgf_data.ImportData(full_path);

			sgf_list.push_back(tmp_sgf_data);
			sgf_list_cnt++;
			if(sgf_list_cnt % 10000 == 0) std::cerr << ".";
		}
	} while (entry != NULL);
	closedir(dr);
	return sgf_list_cnt;

}
#endif // _WIN32

