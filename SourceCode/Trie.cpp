#include "Trie.hpp"

using namespace std;

// Vị trí hiện tại của người dùng, dùng để tính khoảng cách khi chấm điểm
// gợi ý. Mặc định (0, 0); bên ngoài có thể gán lại (khai báo extern trong Trie.hpp).
double userPosX = 0.0, userPosY = 0.0;

/*
 * AutoComplete::ConvertFixed
 * Chức năng : Tạo bản sao của địa điểm với tên đã chuẩn hóa về chữ thường,
 *             không dấu (ví dụ "Đà Nẵng" -> "da nang").
 * Cách làm  : Sao chép toàn bộ Location, sau đó chỉ thay name bằng
 *             toLower(location.name); posX, posY, population giữ nguyên.
 * Trả về    : Location mới đã chuẩn hóa tên.
 */
Location AutoComplete::ConvertFixed(Location location){
    Location fixedLocation = location;
    fixedLocation.name = toLower(location.name);
    return fixedLocation;
}

/*
 * AutoComplete::UpdateSuggestLocation
 * Chức năng : Thêm một địa điểm vào danh sách gợi ý của một nút Trie, sắp
 *             xếp lại theo điểm và chỉ giữ tối đa MAXSUGGESTLOCATION (5)
 *             địa điểm tốt nhất.
 * Các bước  :
 *   1) Thêm location (tên gốc) vào node->topSuggestLocation.
 *   2) Tạo bản chuẩn hóa bằng ConvertFixed rồi thêm vào
 *      node->fixedTopSuggestLocation (hai danh sách luôn song song nhau).
 *   3) Định nghĩa hàm tính điểm calculateScore cho một địa điểm:
 *        điểm = 0.2 * population - 0.8 * khoảng cách từ người dùng
 *        (khoảng cách Euclid tới (userPosX, userPosY) bằng std::hypot).
 *      Địa điểm đông / phổ biến và ở gần người dùng thì điểm cao.
 *   4) Sắp xếp cả hai danh sách theo điểm GIẢM dần; nếu hai điểm bằng nhau
 *      thì xếp theo tên theo thứ tự từ điển tăng dần.
 *   5) Nếu danh sách dài hơn 5 phần tử thì bỏ phần tử cuối (điểm thấp nhất)
 *      của cả hai danh sách.
 * Tham số   : node     - nút Trie cần cập nhật.
 *             location - địa điểm mới cần thêm.
 * Lưu ý     : Điểm được tính theo vị trí người dùng TẠI THỜI ĐIỂM gọi hàm.
 *             Nếu userPosX / userPosY thay đổi về sau, các danh sách đã lưu
 *             không tự được tính lại thứ tự. Địa điểm đã bị loại khỏi top 5
 *             của nút cũng không quay lại được.
 */
void AutoComplete::UpdateSuggestLocation(TrieNode* node, const Location& location){
    node->topSuggestLocation.push_back(location);
    
    Location fixedLocation = ConvertFixed(location);

    node->fixedTopSuggestLocation.push_back(fixedLocation);

    auto calculateScore = [](const Location& location){
        double distance = std::hypot(userPosX - location.posX, userPosY - location.posY);
        return 0.2*location.population - 0.8*distance;
    };

    //Sort suggest locations by population and dictionary 
    std::sort(node->topSuggestLocation.begin(), node->topSuggestLocation.end(), [&](Location& first, Location& second){
        double scoreFirst = calculateScore(first);
        double scoreSecond = calculateScore(second);
        if(scoreFirst == scoreSecond){
            return first.name < second.name;
        }
        return scoreFirst > scoreSecond;
    });

    std::sort(node->fixedTopSuggestLocation.begin(), node->fixedTopSuggestLocation.end(), [&](Location& first, Location& second){
        double scoreFirst = calculateScore(first);
        double scoreSecond = calculateScore(second);
        if(scoreFirst == scoreSecond){
            return first.name < second.name;
        }
        return scoreFirst > scoreSecond;
    });

    //Limit top suggest locations to 5, pop the least population one
    if(node->topSuggestLocation.size() > MAXSUGGESTLOCATION){
        node->topSuggestLocation.pop_back();
        node->fixedTopSuggestLocation.pop_back();
    }
}

/*
 * AutoComplete::InsertLocation
 * Chức năng : Thêm một địa điểm vào cây Trie để sau này có thể gợi ý theo tiền tố.
 * Các bước  :
 *   1) Bắt đầu từ nút gốc và cập nhật danh sách gợi ý của gốc (gốc ứng với
 *      tiền tố rỗng, nên chứa gợi ý tốt nhất của toàn bộ hệ thống).
 *   2) Chuẩn hóa tên địa điểm bằng toLower (chữ thường, không dấu).
 *   3) Với từng ký tự của tên đã chuẩn hóa: nếu nút con tương ứng chưa có
 *      thì tạo mới; đi xuống nút con đó; rồi cập nhật danh sách gợi ý của
 *      nút con (vì địa điểm này khớp với tiền tố tính đến ký tự này).
 *   4) Ở nút cuối cùng: đặt endOfWord = true và lưu location (tên đầy đủ
 *      kết thúc tại đây).
 * Tham số   : location - địa điểm cần thêm (tên gốc có thể có dấu, chữ hoa).
 * Lưu ý     : Hai địa điểm có tên trùng sau khi chuẩn hóa sẽ cùng kết thúc
 *             tại một nút và địa điểm thêm sau ghi đè trường `location` của
 *             nút đó (nhưng cả hai vẫn có thể nằm trong danh sách gợi ý).
 * Độ phức tạp: O(L * k log k), L là độ dài tên, k <= 6 là kích thước danh sách gợi ý.
 */
void AutoComplete::InsertLocation(const Location& location){
    TrieNode* current = root;
    
    UpdateSuggestLocation(root, location);

    std::string fixedLocationName = toLower(location.name);

    for(char ch : fixedLocationName){
        if(current->children.find(ch) == current->children.end()){
            current->children[ch] = new TrieNode;
        }
        current = current->children[ch];

        UpdateSuggestLocation(current, location);
    }

    current->endOfWord = true;
    current->location = location;
}

/*
 * AutoComplete::SearchPrefix
 * Chức năng : Lấy danh sách địa điểm gợi ý cho tiền tố người dùng đang gõ.
 * Các bước  :
 *   1) Chuẩn hóa tiền tố bằng toLower (không phân biệt hoa thường, dấu).
 *   2) Đi từ gốc theo từng ký tự của tiền tố. Nếu thiếu nút con ở bất kỳ
 *      ký tự nào thì không có địa điểm nào khớp, trả về vector rỗng.
 *   3) Nếu đi hết tiền tố, trả về danh sách topSuggestLocation đã lưu sẵn
 *      ở nút đó (không cần duyệt thêm cây con).
 * Tham số   : prefix - tiền tố cần tìm.
 * Trả về    : Tối đa 5 địa điểm (tên gốc có dấu) theo điểm giảm dần; rỗng
 *             nếu không có kết quả. Tiền tố rỗng trả về gợi ý ở nút gốc.
 * Độ phức tạp: O(L) với L là độ dài tiền tố.
 */
vector<Location> AutoComplete::SearchPrefix(std::string prefix){
    TrieNode* current = root;

    std::string fixedPrefix = toLower(prefix);

    for(char ch : fixedPrefix){
        if(current->children.find(ch) == current->children.end()){
            return {};
        }
        current = current->children[ch];
    }

    return current->topSuggestLocation;
}

/*
 * AutoComplete::FreeNode
 * Chức năng : Giải phóng bộ nhớ của một nút và toàn bộ cây con.
 * Cách làm  : Nếu node là nullptr thì thoát. Ngược lại gọi đệ quy FreeNode
 *             cho từng nút con trong children, sau đó delete chính nút này
 *             (xóa con trước, cha sau).
 */
void AutoComplete::FreeNode(TrieNode* node){
    if(!node) return;
    for(auto& item : node->children) FreeNode(item.second);
    delete node;
}

/*
 * AutoComplete::Clear
 * Chức năng : Xóa toàn bộ dữ liệu Trie, đưa về trạng thái rỗng như lúc mới tạo.
 * Cách làm  : Gọi FreeNode(root) để giải phóng cả cây, rồi tạo lại một nút
 *             gốc mới để đối tượng vẫn dùng tiếp được.
 */
void AutoComplete::Clear(){
    FreeNode(root);
    root = new TrieNode;
}

/*
 * AutoComplete::Traverse
 * Chức năng : Duyệt đệ quy theo chiều sâu (DFS) toàn bộ cây con của một nút,
 *             gọi callback visit cho từng nút.
 * Các bước  :
 *   1) Gọi visit(prefix, *node) cho nút hiện tại (thứ tự trước: cha trước con).
 *   2) Với mỗi nút con: thêm ký tự của nhánh vào prefix (push_back), đệ quy
 *      xuống nút con, rồi bỏ ký tự đó đi (pop_back) để prefix trở lại như cũ
 *      trước khi sang nhánh anh em kế tiếp.
 * Tham số   : node   - nút đang duyệt.
 *             prefix - tiền tố từ gốc tới node (truyền tham chiếu, được sửa
 *                      rồi hoàn trả trong lúc đệ quy).
 *             visit  - hàm callback xử lý từng nút.
 * Lưu ý     : Thứ tự các nhánh con không cố định vì children là unordered_map.
 */
void AutoComplete::Traverse(const TrieNode* node, std::string& prefix,
                            const std::function<void(const std::string&, const TrieNode&)>& visit) const{
    visit(prefix, *node);
    for(const auto& item : node->children){
        prefix.push_back(item.first);
        Traverse(item.second, prefix, visit);
        prefix.pop_back();
    }
}

/*
 * AutoComplete::ForEachNode
 * Chức năng : Hàm công khai để duyệt toàn bộ nút của Trie (bao gồm nút gốc
 *             với tiền tố rỗng) và gọi visit cho từng nút.
 * Cách làm  : Tạo chuỗi prefix rỗng rồi gọi Traverse bắt đầu từ root.
 * Tham số   : visit - callback nhận (tiền tố của nút, tham chiếu const tới nút).
 */
void AutoComplete::ForEachNode(const std::function<void(const std::string&, const TrieNode&)>& visit) const{
    std::string prefix;
    Traverse(root, prefix, visit);
}