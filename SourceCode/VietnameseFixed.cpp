#include "VietnameseFixed.hpp"
using namespace std;

/*
 * toLower
 * Chức năng : Chuyển chuỗi UTF-8 sang chữ thường và bỏ dấu tiếng Việt
 *             (ví dụ "Nguyễn Văn Đức" -> "nguyen van duc").
 * Ý tưởng   : Vì std::string chứa các byte UTF-8, một chữ tiếng Việt có dấu
 *             chiếm 2 đến 3 byte nên không thể xử lý từng byte một. Hàm giải
 *             mã từng ký tự thành mã Unicode (codepoint), rồi tra bảng
 *             charMap để đổi thành chữ cái ASCII không dấu.
 * Các bước  :
 *   1) Khai báo bảng ánh xạ charMap (static, chỉ khởi tạo một lần): mỗi
 *      ký tự tiếng Việt có dấu (cả hoa lẫn thường) ánh xạ về chữ thường
 *      không dấu tương ứng.
 *   2) Duyệt chuỗi đầu vào theo từng ký tự UTF-8: đọc byte đầu để biết ký
 *      tự dài mấy byte rồi ghép các byte lại thành codepoint.
 *   3) Tra codepoint trong charMap:
 *        - Có trong bảng: thêm chữ cái không dấu tương ứng vào kết quả.
 *        - Không có và là ký tự ASCII (<= 0x7F): đổi thường bằng tolower
 *          rồi thêm vào kết quả (số, khoảng trắng, dấu câu giữ nguyên).
 *        - Còn lại (không phải ASCII và không có trong bảng): bỏ qua.
 * Tham số   : input - chuỗi UTF-8 cần chuẩn hóa (không bị sửa đổi).
 * Trả về    : Chuỗi kết quả chỉ gồm ký tự ASCII chữ thường.
 * Lưu ý     : Bảng chỉ chứa ký tự dựng sẵn (một mã Unicode cho mỗi chữ có
 *             dấu, dạng NFC). Độ phức tạp O(n) với n là số byte của chuỗi.
 */
string toLower(const string& input) {
    // Bảng ánh xạ: key = mã Unicode của chữ tiếng Việt có dấu (hoa hoặc
    // thường), value = chữ cái ASCII thường không dấu tương ứng.
    static const unordered_map<char32_t , char> charMap = {
        // Nhóm chữ A: a, ă, â cùng các dấu (huyền, sắc, hỏi, ngã, nặng) - hoa và thường -> 'a'
        {U'à', 'a'}, {U'á', 'a'}, {U'ả', 'a'}, {U'ã', 'a'}, {U'ạ', 'a'},
        {U'ă', 'a'}, {U'ằ', 'a'}, {U'ắ', 'a'}, {U'ẳ', 'a'}, {U'ẵ', 'a'}, {U'ặ', 'a'},
        {U'â', 'a'}, {U'ầ', 'a'}, {U'ấ', 'a'}, {U'ẩ', 'a'}, {U'ẫ', 'a'}, {U'ậ', 'a'},
        {U'À', 'a'}, {U'Á', 'a'}, {U'Ả', 'a'}, {U'Ã', 'a'}, {U'Ạ', 'a'},
        {U'Ă', 'a'}, {U'Ằ', 'a'}, {U'Ắ', 'a'}, {U'Ẳ', 'a'}, {U'Ẵ', 'a'}, {U'Ặ', 'a'},
        {U'Â', 'a'}, {U'Ầ', 'a'}, {U'Ấ', 'a'}, {U'Ẩ', 'a'}, {U'Ẫ', 'a'}, {U'Ậ', 'a'},

        // Chữ Đ / đ -> 'd'
        {U'đ', 'd'}, {U'Đ', 'd'},

        // Nhóm chữ E: e, ê cùng các dấu - hoa và thường -> 'e'
        {U'è', 'e'}, {U'é', 'e'}, {U'ẻ', 'e'}, {U'ẽ', 'e'}, {U'ẹ', 'e'},
        {U'ê', 'e'}, {U'ề', 'e'}, {U'ế', 'e'}, {U'ể', 'e'}, {U'ễ', 'e'}, {U'ệ', 'e'},
        {U'È', 'e'}, {U'É', 'e'}, {U'Ẻ', 'e'}, {U'Ẽ', 'e'}, {U'Ẹ', 'e'},
        {U'Ê', 'e'}, {U'Ề', 'e'}, {U'Ế', 'e'}, {U'Ể', 'e'}, {U'Ễ', 'e'}, {U'Ệ', 'e'},

        // Nhóm chữ I: i cùng các dấu - hoa và thường -> 'i'
        {U'ì', 'i'}, {U'í', 'i'}, {U'ỉ', 'i'}, {U'ĩ', 'i'}, {U'ị', 'i'},
        {U'Ì', 'i'}, {U'Í', 'i'}, {U'Ỉ', 'i'}, {U'Ĩ', 'i'}, {U'Ị', 'i'},

        // Nhóm chữ O: o, ô, ơ cùng các dấu - hoa và thường -> 'o'
        {U'ò', 'o'}, {U'ó', 'o'}, {U'ỏ', 'o'}, {U'õ', 'o'}, {U'ọ', 'o'},
        {U'ô', 'o'}, {U'ồ', 'o'}, {U'ố', 'o'}, {U'ổ', 'o'}, {U'ỗ', 'o'}, {U'ộ', 'o'},
        {U'ơ', 'o'}, {U'ờ', 'o'}, {U'ớ', 'o'}, {U'ở', 'o'}, {U'ỡ', 'o'}, {U'ợ', 'o'},
        {U'Ò', 'o'}, {U'Ó', 'o'}, {U'Ỏ', 'o'}, {U'Õ', 'o'}, {U'Ọ', 'o'},
        {U'Ô', 'o'}, {U'Ồ', 'o'}, {U'Ố', 'o'}, {U'Ổ', 'o'}, {U'Ỗ', 'o'}, {U'Ộ', 'o'},
        {U'Ơ', 'o'}, {U'Ờ', 'o'}, {U'Ớ', 'o'}, {U'Ở', 'o'}, {U'Ỡ', 'o'}, {U'Ợ', 'o'},

        // Nhóm chữ U: u, ư cùng các dấu - hoa và thường -> 'u'
        {U'ù', 'u'}, {U'ú', 'u'}, {U'ủ', 'u'}, {U'ũ', 'u'}, {U'ụ', 'u'},
        {U'ư', 'u'}, {U'ừ', 'u'}, {U'ứ', 'u'}, {U'ử', 'u'}, {U'ữ', 'u'}, {U'ự', 'u'},
        {U'Ù', 'u'}, {U'Ú', 'u'}, {U'Ủ', 'u'}, {U'Ũ', 'u'}, {U'Ụ', 'u'},
        {U'Ư', 'u'}, {U'Ừ', 'u'}, {U'Ứ', 'u'}, {U'Ử', 'u'}, {U'Ữ', 'u'}, {U'Ự', 'u'},

        // Nhóm chữ Y: y cùng các dấu - hoa và thường -> 'y'
        {U'ỳ', 'y'}, {U'ý', 'y'}, {U'ỷ', 'y'}, {U'ỹ', 'y'}, {U'ỵ', 'y'},
        {U'Ỳ', 'y'}, {U'Ý', 'y'}, {U'Ỷ', 'y'}, {U'Ỹ', 'y'}, {U'Ỵ', 'y'}
    };

    string result = "";   // Chuỗi kết quả, được nối dần từng ký tự.
    size_t i = 0;         // Vị trí byte hiện tại trong chuỗi đầu vào.
    size_t len = input.length();

    while(i < len){
        char32_t codepoint = 0;   // Mã Unicode của ký tự đang xử lý.
        unsigned char c = input[i];

        // Giải mã UTF-8: byte đầu cho biết ký tự dài bao nhiêu byte.
        // (Điều kiện i + k < len đảm bảo không đọc vượt quá cuối chuỗi.)
        if(c <= 0x7F){
            // Dạng 0xxxxxxx: ký tự ASCII, 1 byte.
            codepoint = c;
            i += 1;
        }else if((c & 0xe0) == 0xC0 && i + 1 < len){
            // Dạng 110xxxxx 10xxxxxx: ký tự 2 byte (ví dụ à, á, ă, â, đ...).
            codepoint = ((c & 0x1F) << 6) | (input[i + 1] & 0x3F);
            i += 2;
        }else if((c & 0xF0) == 0xe0 && i + 2 < len){
            // Dạng 1110xxxx 10xxxxxx 10xxxxxx: ký tự 3 byte (ví dụ ả, ạ, ế, ệ...).
            codepoint = ((c & 0x0F) << 12) | ((input[i + 1] & 0x3F) << 6) | (input[i + 2] & 0x3F);
            i += 3;
        }else if((c & 0xF8) == 0xF0 && i + 3 < len){
            // Dạng 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx: ký tự 4 byte (ví dụ emoji).
            codepoint = ((c & 0x07) << 18) | ((input[i + 1] & 0x3F) << 12) | ((input[i + 2] & 0x3F) << 6) | (input[i + 3] & 0x3F);
            i += 4;
        }else{
            // Byte không hợp lệ hoặc ký tự bị cắt ngang cuối chuỗi: bỏ qua 1 byte.
            i += 1;
            continue;
        }

        // Tìm kiếm ký tự trong bảng ánh xạ
        auto it = charMap.find(codepoint);
        if(it != charMap.end()){
            // Là chữ tiếng Việt có dấu: thêm chữ cái không dấu tương ứng.
            result += it->second;
        }else if (codepoint <= 0x7F){
            // Là ký tự ASCII: đổi sang chữ thường (số, dấu câu không đổi).
            result += static_cast<char>(tolower(static_cast<unsigned char>(codepoint)));
        }
        // Các trường hợp còn lại (không phải ASCII, không có trong bảng) bị bỏ qua.
    }

    return result;
}