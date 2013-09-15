function CWATERLEVEL(conf) {
  this.isResizable=true;
  this.initialized = false;
  this.init(conf);
  this.refreshHTML();
}

CWATERLEVEL.type='waterlevel';
UIController.registerWidget(CWATERLEVEL);
CWATERLEVEL.prototype = new CWidget();

// Refresh HTML from config
CWATERLEVEL.prototype.refreshHTML = function() {
    var link = "widgets/waterlevel/waterlevel.html";
    this.iframe = document.createElement('iframe');
    this.iframe.frameBorder=0;
    this.iframe.width="100%";
    this.iframe.height="100%";
    this.iframe.id="my_waterlevel_frame";
    this.iframe.setAttribute("src", link);
    $(this.div).append(this.iframe);

    //$("div:first-child", this.div).html("<iframe id='my_waterlevel_frame' src='widgets/waterlevel/waterlevel.html' width='100%' height='100%' frameBorder=0 />");
}

// Called by eibcommunicator when a feedback object value has changed
CWATERLEVEL.prototype.updateObject = function(obj,value) {

	if (obj==this.conf.getAttribute("feedback-object"))
	{
            if (!value)
                return;

            if (!this.initialized)
            {
                var iframe = this.iframe; //document.getElementById("my_waterlevel_frame");
                if (iframe)
                {
                    if (iframe.contentWindow.setLabel)
                    {
                        iframe.contentWindow.setLabel(this.conf.getAttribute("label"));
                        var waterHeight = parseInt(this.conf.getAttribute("maxDepth")) -
                                          parseInt(this.conf.getAttribute("maxWater"));
                        var volume = parseInt(this.conf.getAttribute("diameter")) / 200.00;
                        volume = Math.pow(volume, 2);
                        volume *= Math.PI;
                        volume *= (waterHeight / 100.00);
                        iframe.contentWindow.setMax(Math.round(volume * 1000));
                        this.initialized = true;
                    }
                }
            }

            //if (this.currValue == value)
            //    return;
            
            var iframe = this.iframe; //document.getElementById("my_waterlevel_frame");
            
            if (iframe.contentWindow.setValue)
            {
               this.currValue = value;

               var waterHeight = parseInt(this.conf.getAttribute("maxDepth")) - parseInt(value);
               var volume = parseInt(this.conf.getAttribute("diameter")) / 200.00;
               volume = Math.pow(volume, 2);
               volume *= Math.PI;
               volume *= (waterHeight / 100.00);
               iframe.contentWindow.setValue(Math.round(volume * 1000));
            }
	}
};
