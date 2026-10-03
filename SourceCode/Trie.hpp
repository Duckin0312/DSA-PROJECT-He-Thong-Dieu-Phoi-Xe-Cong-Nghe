#pragma once
#include "VietnameseFixed.hpp"
#include <algorithm>
#include <vector>
#include <cmath>
#include <unordered_map>
#include <functional>

/*
 * Struct Location
 * ---------------------------------------------------------------
 * Mục đích: Thông tin của một địa điểm có thể được gợi ý trong ô tìm kiếm.
 */
struct Location{
    std::string name;   // Tên địa điểm (giữ nguyên chữ hoa, dấu tiếng Việt).
    double posX;        // Tọa độ x của địa điểm.
    double posY;        // Tọa độ y của địa điểm.
    int population;     // Mức độ phổ biến (dân số / lượng người), dùng khi tính điểm gợi ý.
};

/*
 * Struct TrieNode
 * ---------------------------------------------------------------
 * Mục đích: Một nút của cây Trie. Mỗi nút ứng với một tiền tố (đường đi từ
 * gốc đến nút). Tiền tố được xây từ tên địa điểm đã chuẩn hóa (chữ thường,
 * không dấu).
 */
struct TrieNode{
    // Các nút con: key = ký tự tiếp theo, value = con trỏ tới nút con.
    std::unordered_map<char, TrieNode*> children;
    // true nếu đường đi từ gốc đến nút này là tên đầy đủ của một địa điểm.
    bool endOfWord = false;
    // Địa điểm có tên đầy đủ kết thúc tại nút này (chỉ có nghĩa khi endOfWord = true).
    Location location;
    // Tối đa 5 địa điểm được gợi ý tốt nhất cho tiền tố của nút này (tên gốc có dấu),
    // đã sắp xếp theo điểm giảm dần.
    std::vector<Location> topSuggestLocation;
    // Danh sách tương tự topSuggestLocation nhưng tên đã chuẩn hóa (chữ thường, không dấu).
    std::vector<Location> fixedTopSuggestLocation;
};

// Vị trí hiện tại của người dùng (biến toàn cục, định nghĩa trong Trie.cpp).
// Dùng để tính khoảng cách khi chấm điểm địa điểm gợi ý.
extern double userPosX, userPosY;

/*
 * Lớp AutoComplete
 * ---------------------------------------------------------------
 * Mục đích: Gợi ý địa điểm tự động khi người dùng gõ (autocomplete), dựa
 * trên cây Trie. Mỗi nút của Trie lưu sẵn danh sách tối đa 5 địa điểm tốt
 * nhất cho tiền tố của nó, nên tra cứu gợi ý chỉ cần đi theo các ký tự của
 * tiền tố, không phải duyệt cả cây.
 * Việc so khớp không phân biệt hoa thường và không phân biệt dấu tiếng Việt
 * (nhờ hàm toLower trong VietnameseFixed).
 * Lớp quản lý bộ nhớ động (new / delete) nên bị cấm sao chép.
 */
class AutoComplete{
    private:
        // Nút gốc của cây Trie (đại diện cho tiền tố rỗng).
        TrieNode* root;
        // Số lượng gợi ý tối đa lưu ở mỗi nút.
        const int MAXSUGGESTLOCATION = 5;

        /*
         * FreeNode
         * Chức năng : Giải phóng (delete) một nút cùng toàn bộ cây con của nó bằng đệ quy.
         * Tham số   : node - nút gốc của nhánh cần giải phóng; nullptr thì bỏ qua.
         */
        void FreeNode(TrieNode* node);

        /*
         * Traverse
         * Chức năng : Duyệt đệ quy (theo chiều sâu) toàn bộ cây con từ một nút,
         *             gọi hàm visit cho từng nút gặp được.
         * Tham số   : node   - nút hiện tại đang duyệt.
         *             prefix - chuỗi tiền tố từ gốc tới nút hiện tại (được
         *                      cập nhật và hoàn trả khi đệ quy).
         *             visit  - hàm callback nhận (tiền tố, nút) và được gọi
         *                      cho mọi nút.
         * Lưu ý     : Hàm private; bên ngoài dùng ForEachNode.
         */
        void Traverse(const TrieNode* node, std::string& prefix,
                      const std::function<void(const std::string&, const TrieNode&)>& visit) const;

    public:
        /*
         * AutoComplete (hàm khởi tạo)
         * Chức năng : Tạo cây Trie rỗng, chỉ gồm nút gốc.
         */
        AutoComplete(){ root = new TrieNode; }

        /*
         * ~AutoComplete (hàm hủy)
         * Chức năng : Giải phóng toàn bộ cây Trie (gọi FreeNode từ gốc) để tránh rò rỉ bộ nhớ.
         */
        ~AutoComplete(){ FreeNode(root); }

        /*
         * AutoComplete (hàm khởi tạo sao chép) - đã bị xóa (= delete)
         * Chức năng : Cấm sao chép đối tượng. Lớp giữ con trỏ thô tới cây
         *             cấp phát động; nếu sao chép nông, hai đối tượng sẽ
         *             cùng giải phóng một cây gây lỗi double free.
         */
        AutoComplete(const AutoComplete&) = delete;

        /*
         * operator= (toán tử gán sao chép) - đã bị xóa (= delete)
         * Chức năng : Cấm gán đối tượng này cho đối tượng khác, cùng lý do như trên.
         */
        AutoComplete& operator=(const AutoComplete&) = delete;

        /*
         * ConvertFixed
         * Chức năng : Tạo bản sao của Location với tên đã chuẩn hóa (chữ
         *             thường, không dấu); các trường khác giữ nguyên.
         * Tham số   : location - địa điểm gốc.
         * Trả về    : Location mới có name = toLower(name gốc).
         */
        Location ConvertFixed(Location location);

        /*
         * UpdateSuggestLocation
         * Chức năng : Thêm một địa điểm vào danh sách gợi ý của một nút, sắp
         *             xếp lại theo điểm và giữ tối đa 5 địa điểm tốt nhất.
         * Tham số   : node     - nút Trie cần cập nhật danh sách gợi ý.
         *             location - địa điểm mới cần thêm.
         */
        void UpdateSuggestLocation(TrieNode* node, const Location& location);

        /*
         * InsertLocation
         * Chức năng : Thêm một địa điểm vào cây Trie: tạo các nút theo từng ký
         *             tự của tên (đã chuẩn hóa), cập nhật danh sách gợi ý của
         *             mọi nút trên đường đi và đánh dấu nút cuối là kết thúc từ.
         * Tham số   : location - địa điểm cần thêm.
         */
        void InsertLocation(const Location& location);

        /*
         * SearchPrefix
         * Chức năng : Lấy danh sách địa điểm gợi ý cho một tiền tố người dùng đã gõ.
         * Tham số   : prefix - tiền tố cần tìm (không phân biệt hoa thường và dấu).
         * Trả về    : Tối đa 5 địa điểm (tên gốc, có dấu) đã xếp theo điểm giảm
         *             dần; vector rỗng nếu không có địa điểm nào khớp.
         */
        std::vector<Location> SearchPrefix(std::string prefix);

        // Mới
        /*
         * Clear
         * Chức năng : Xóa toàn bộ dữ liệu trong Trie và đưa về trạng thái rỗng
         *             (giải phóng cây cũ rồi tạo lại nút gốc mới).
         */
        void Clear();

        /*
         * ForEachNode
         * Chức năng : Duyệt toàn bộ các nút của Trie (kể cả nút gốc), gọi hàm
         *             visit cho từng nút. Dùng để đọc dữ liệu từ bên ngoài
         *             (ví dụ lưu Trie ra file hoặc thống kê).
         * Tham số   : visit - callback nhận (tiền tố của nút, tham chiếu const tới nút).
         * Lưu ý     : Thứ tự duyệt các nút con không được đảm bảo (unordered_map).
         */
        void ForEachNode(const std::function<void(const std::string&, const TrieNode&)>& visit) const;

        /*
         * GetMaxSuggest
         * Chức năng : Trả về số gợi ý tối đa mỗi nút lưu (hiện là 5).
         */
        int GetMaxSuggest() const { return MAXSUGGESTLOCATION; }
};