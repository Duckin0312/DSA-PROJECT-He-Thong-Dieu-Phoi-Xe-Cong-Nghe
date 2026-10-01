#include "VietnameseFixed.hpp"
using namespace std;

string toLower(const string& input) {
    static const unordered_map<char32_t , char> charMap = {
        {U'à', 'a'}, {U'á', 'a'}, {U'ả', 'a'}, {U'ã', 'a'}, {U'ạ', 'a'},
        {U'ă', 'a'}, {U'ằ', 'a'}, {U'ắ', 'a'}, {U'ẳ', 'a'}, {U'ẵ', 'a'}, {U'ặ', 'a'},
        {U'â', 'a'}, {U'ầ', 'a'}, {U'ấ', 'a'}, {U'ẩ', 'a'}, {U'ẫ', 'a'}, {U'ậ', 'a'},
        {U'À', 'a'}, {U'Á', 'a'}, {U'Ả', 'a'}, {U'Ã', 'a'}, {U'Ạ', 'a'},
        {U'Ă', 'a'}, {U'Ằ', 'a'}, {U'Ắ', 'a'}, {U'Ẳ', 'a'}, {U'Ẵ', 'a'}, {U'Ặ', 'a'},
        {U'Â', 'a'}, {U'Ầ', 'a'}, {U'Ấ', 'a'}, {U'Ẩ', 'a'}, {U'Ẫ', 'a'}, {U'Ậ', 'a'},

        {U'đ', 'd'}, {U'Đ', 'd'},

        {U'è', 'e'}, {U'é', 'e'}, {U'ẻ', 'e'}, {U'ẽ', 'e'}, {U'ẹ', 'e'},
        {U'ê', 'e'}, {U'ề', 'e'}, {U'ế', 'e'}, {U'ể', 'e'}, {U'ễ', 'e'}, {U'ệ', 'e'},
        {U'È', 'e'}, {U'É', 'e'}, {U'Ẻ', 'e'}, {U'Ẽ', 'e'}, {U'Ẹ', 'e'},
        {U'Ê', 'e'}, {U'Ề', 'e'}, {U'Ế', 'e'}, {U'Ể', 'e'}, {U'Ễ', 'e'}, {U'Ệ', 'e'},

        {U'ì', 'i'}, {U'í', 'i'}, {U'ỉ', 'i'}, {U'ĩ', 'i'}, {U'ị', 'i'},
        {U'Ì', 'i'}, {U'Í', 'i'}, {U'Ỉ', 'i'}, {U'Ĩ', 'i'}, {U'Ị', 'i'},

        {U'ò', 'o'}, {U'ó', 'o'}, {U'ỏ', 'o'}, {U'õ', 'o'}, {U'ọ', 'o'},
        {U'ô', 'o'}, {U'ồ', 'o'}, {U'ố', 'o'}, {U'ổ', 'o'}, {U'ỗ', 'o'}, {U'ộ', 'o'},
        {U'ơ', 'o'}, {U'ờ', 'o'}, {U'ớ', 'o'}, {U'ở', 'o'}, {U'ỡ', 'o'}, {U'ợ', 'o'},
        {U'Ò', 'o'}, {U'Ó', 'o'}, {U'Ỏ', 'o'}, {U'Õ', 'o'}, {U'Ọ', 'o'},
        {U'Ô', 'o'}, {U'Ồ', 'o'}, {U'Ố', 'o'}, {U'Ổ', 'o'}, {U'Ỗ', 'o'}, {U'Ộ', 'o'},
        {U'Ơ', 'o'}, {U'Ờ', 'o'}, {U'Ớ', 'o'}, {U'Ở', 'o'}, {U'Ỡ', 'o'}, {U'Ợ', 'o'},

        {U'ù', 'u'}, {U'ú', 'u'}, {U'ủ', 'u'}, {U'ũ', 'u'}, {U'ụ', 'u'},
        {U'ư', 'u'}, {U'ừ', 'u'}, {U'ứ', 'u'}, {U'ử', 'u'}, {U'ữ', 'u'}, {U'ự', 'u'},
        {U'Ù', 'u'}, {U'Ú', 'u'}, {U'Ủ', 'u'}, {U'Ũ', 'u'}, {U'Ụ', 'u'},
        {U'Ư', 'u'}, {U'Ừ', 'u'}, {U'Ứ', 'u'}, {U'Ử', 'u'}, {U'Ữ', 'u'}, {U'Ự', 'u'},

        {U'ỳ', 'y'}, {U'ý', 'y'}, {U'ỷ', 'y'}, {U'ỹ', 'y'}, {U'ỵ', 'y'},
        {U'Ỳ', 'y'}, {U'Ý', 'y'}, {U'Ỷ', 'y'}, {U'Ỹ', 'y'}, {U'Ỵ', 'y'}
    };

    string result = "";
    size_t i = 0;
    size_t len = input.length();

    while(i < len){
        char32_t codepoint = 0;
        unsigned char c = input[i];

        //Solve UTF-8 code point
        if(c <= 0x7F){
            codepoint = c;
            i += 1;
        }else if((c & 0xe0) == 0xC0 && i + 1 < len){
            codepoint = ((c & 0x1F) << 6) | (input[i + 1] & 0x3F);
            i += 2;
        }else if((c & 0xF0) == 0xe0 && i + 2 < len){
            codepoint = ((c & 0x0F) << 12) | ((input[i + 1] & 0x3F) << 6) | (input[i + 2] & 0x3F);
            i += 3;
        }else if((c & 0xF8) == 0xF0 && i + 3 < len){
            codepoint = ((c & 0x07) << 18) | ((input[i + 1] & 0x3F) << 12) | ((input[i + 2] & 0x3F) << 6) | (input[i + 3] & 0x3F);
            i += 4;
        }else{
            i += 1;
            continue;
        }

        // Tìm kiếm ký tự trong bảng ánh xạ
        auto it = charMap.find(codepoint);
        if(it != charMap.end()){
            result += it->second;
        }else if (codepoint <= 0x7F){
            result += static_cast<char>(tolower(static_cast<unsigned char>(codepoint)));
        }
    }

    return result;
}