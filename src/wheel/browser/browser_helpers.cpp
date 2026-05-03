#include "wheel/browser/browser_helpers.h"
#include <json.hpp>

namespace lynne {
namespace wheel {

static const char* kDumpJs = R"JSON_END(
JSON.stringify({u:document.URL,t:(document.title||'').slice(0,100),
n:Array.from(document.querySelectorAll('*')).filter(function(e){
 return e.id||(e.className&&typeof e.className==='string'&&e.className.trim())||e.getAttribute('role')||e.getAttribute('aria-label')||e.href||e.tagName==='INPUT'||e.tagName==='BUTTON'||e.tagName==='A'||e.tagName==='FORM'||e.tagName==='TEXTAREA'||e.tagName==='SELECT'
}).slice(0,200).map(function(e){
 var o={t:e.tagName.toLowerCase()};
 if(e.id)o.i=e.id;
 var c=(typeof e.className==='string'?e.className:'').trim().slice(0,80);if(c)o.c=c;
 var r=e.getAttribute('role');if(r)o.r=r;
 var l=e.getAttribute('aria-label');if(l)o.l=l;
 var d=e.getAttribute('data-testid');if(d)o.d=d;
 if(e.href)o.h=e.href;
 if(e.tagName==='INPUT'){var tp=e.getAttribute('type'),p=e.getAttribute('placeholder');if(tp)o.ty=tp;if(p)o.p=p;}
 if(e.tagName==='BUTTON'){var bt=e.getAttribute('type');if(bt)o.ty=bt;}
 if(e.tagName==='FORM'){var ac=e.getAttribute('action');if(ac)o.a=ac;}
 var tx=(e.textContent||'').trim().slice(0,60);if(tx&&(e.tagName==='A'||e.tagName==='BUTTON'||e.tagName==='SPAN'||e.tagName==='P'||e.tagName==='H1'||e.tagName==='H2'||e.tagName==='H3'||e.tagName==='LI'||e.tagName==='LABEL'||e.tagName==='DIV'))o.x=tx;
 return o;
})})
)JSON_END";

void dump_page_structure(
    BrowserContext* ctx,
    std::function<void(const std::string&)> on_result,
    std::function<void(const std::string&)> on_error) {
    ctx->evaluate(kDumpJs,
        [on_result](const nlohmann::json& r) {
            if (r.contains("value") && !r["value"].is_null()) {
                on_result(r["value"].get<std::string>());
            } else {
                on_result("{}");
            }
        },
        [on_error](const std::string& err) {
            on_error(err);
        });
}

} // namespace wheel
} // namespace lynne
