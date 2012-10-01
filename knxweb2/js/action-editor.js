var actionEditor = {
	config: null,
	isNew: false,
	subPageObjects: [ ],
  prefix: '',

	actionsList: {
	    'set-value' : 'Set Value', // < type="" id="" value="" />
	    'copy-value' : 'Copy Value', // < type="" from="" to="" />
	    'toggle-value' : 'Toggle Value', // < type="" id="" />
	    'set-string' : 'Set String', // < type="" id="" value="" />
	    'send-read-request' : 'Send Read Request', // < type="" id="" />
	    'cycle-on-off' : 'Cycle On Off', // < type="" id="" on="" off="" count="" ><stopcondition ... />
//	    'repeat' : 'Repeat', // < type="" period="" count="" ><action ... />			// Todo
//	    'conditional' : 'Conditional', // < type="" ><condition ...								// Todo
	    'send-sms' : 'Send Sms', // < type="" id="" value="" var="true/false" />
	    'send-email' : 'Send Email', // <action type="" to="" subject="" var="true/false" >text<action/>
	    'dim-up' : 'Dim Up', // < type="" id="" start="" stop="" duration="" />
	    'shell-cmd' : 'Shell Cmd', // < type="" cmd="" var="true/false" />
	    'ioport-tx' : 'Ioport Tx', // < type="" hex="true/false" data="" ioport="" var="true/false" />
	    'script' : 'Script', // < type="" >script <... />
//	    'cancel' : 'Cancel', // < type="" rule-id="" />
	    'formula' : 'Formula', // id = a*x^m+b*y^n+c < type="" id="object" x="" y="" a="1" b="1" c="0" m="1" n="1" />
	    'start-actionlist' : 'Start actionlist', // < type="" rule-id="" list="true/false" />
      'set-rule-active' : 'Set rule active' // < type="" rule-id="" active="yes/no" /> 
	},
	
	new: function(type) {
		
		var conf=actionEditor.config.ownerDocument.createElement('action');
		conf.setAttribute('type', type);
		//$('actionlist', actionEditor.config).append(conf);
		actionEditor.config.appendChild(conf);
		return actionEditor.add(conf);
	},
	
	add: function(conf) {
		var tr=$("<tr>");

    tr[0].conf=conf;
    
    tr.dblclick(function () {
      actionEditor.edit(this, false);
    });

    tr.append($("<td>" + conf.getAttribute('type') + "</td>"));
    tr.append($("<td class='description'>" + actionEditor.getActionDescription(conf) + "</td>"));
    
    var td=$("<td align='center'></td>");
    var input=$("<input type='text' size='3'>");
    input.change(function() {
    	var tr=$(this).parent().parent();
    	tr.get(0).conf.setAttribute('delay', $(this).val());
    });
    td.append(input);
    
    tr.append(td);
    tr.append($("<td><img src='images/remove.png' onclick='actionEditor.del($(this).parent().parent());'></td>"));

		$("#"+this.prefix+"action-dialog-list tbody").append(tr);

    return tr;
	},
	
	del: function(tr) {
		tr.get(0).conf.parentNode.removeChild(tr.get(0).conf);
		tr.remove();
	},
	
	getActionDescription: function(conf) {
    switch (conf.getAttribute('type')) {
      case 'set-value':
      	return conf.getAttribute('id') + ' to ' + conf.getAttribute('value');
      case 'copy-value':
      	return conf.getAttribute('from') + ' to ' + conf.getAttribute('to');
      case 'toggle-value':
      	return conf.getAttribute('id');
      case 'set-string':
      	return conf.getAttribute('id') + ' to ' + conf.getAttribute('value');
      case 'send-read-request':
      	return conf.getAttribute('id');
      case 'cycle-on-off':
      	return conf.getAttribute('id') + ', ' + conf.getAttribute('on') + '/' + conf.getAttribute('off') + " count " + conf.getAttribute('count');
      case 'send-sms':
      	return conf.getAttribute('id') + ', ' + conf.getAttribute('value');
      case 'send-email':
      	return 'to ' + conf.getAttribute('to') + ', ' + conf.getAttribute('subject');
      case 'dim-up':
      	return conf.getAttribute('id');
      case 'shell-cmd':
      	return conf.getAttribute('cmd');
      case 'ioport-tx':
      	return conf.getAttribute('data') + ' to ' + conf.getAttribute('ioport');
      case 'script':
      	return conf.textContent.substr(0,50) + '...';
      case 'formula':
      	return "object: " + conf.getAttribute('id') + ", x=" + conf.getAttribute('x') + ", y=" + conf.getAttribute('y') + ", a=" + conf.getAttribute('a') + ", b=" + conf.getAttribute('b') + ", c=" + conf.getAttribute('c') + ", m=" + conf.getAttribute('m') + ", n=" + conf.getAttribute('n');
      case 'start-actionlist':
      	return "rule: " + conf.getAttribute('rule-id');
      case 'set-rule-active':
      	return "rule: " + conf.getAttribute('rule-id');
    }
	},
	
  edit: function(tr, isNew) {

    actionEditor.isNew=isNew;
     
		var dialog=$("#action-dialog-" + tr.conf.getAttribute('type') + "-dialog");
		
		dialog[0].editing=tr;
		
		actionEditor.fillObjectsSelect(dialog);
		actionEditor.fillIOPortsSelect(dialog);
		
    switch (tr.conf.getAttribute('type')) {
      case 'set-value' :
        $("[name=id]", dialog).val(tr.conf.getAttribute('id'));
        $("[name=id]", dialog).bind('change', function() {
          if (_objectTypesValues[$("option:selected", this)[0].type])
          {
            values=_objectTypesValues[$("option:selected", this)[0].type];
            $("[name=values]", dialog).empty();
            $(values).each(function() { $("[name=values]", dialog).append('<option value="' + this + '">' + this + '</option>'); });
            $("[name=values]", dialog).show();
            $("[name=value]", dialog).hide();
          } else
          {
            $("[name=values]", dialog).hide();
            $("[name=value]", dialog).show();
          }
        }).trigger('change');
        $("[name=value]", dialog).val(tr.conf.getAttribute('value'));
        $("[name=values]", dialog).val(tr.conf.getAttribute('value'));
        break;
      case 'copy-value' :
        $("[name=from]", dialog).val(tr.conf.getAttribute('from'));
        $("[name=to]", dialog).val(tr.conf.getAttribute('to'));
        break;
      case 'toggle-value' :
        $("[name=id]", dialog).val(tr.conf.getAttribute('id'));
        break;
      case 'set-string' :
        $("[name=id]", dialog).val(tr.conf.getAttribute('id'));
        $("[name=value]", dialog).val(tr.conf.getAttribute('value'));
        break;
      case 'send-read-request' :
        $("[name=id]", dialog).val(tr.conf.getAttribute('id'));
        break;
      case 'cycle-on-off' :
        $("[name=id]", dialog).val(tr.conf.getAttribute('id'));
        $("[name=on]", dialog).val(tr.conf.getAttribute('on'));
        $("[name=off]", dialog).val(tr.conf.getAttribute('off'));
        $("[name=count]", dialog).val(tr.conf.getAttribute('count'));
  // TODO g�rer stopcondition
        break;
      case 'repeat' :
//        $('#tab-rules-repeat-action-period').val(div.period);
//        $('#tab-rules-repeat-action-count').val(div.count);
  // TODO g�rer Action 
        break;
      case 'conditional' : // TODO � compl�ter
        break;
      case 'send-sms' :
        $("[name=id]", dialog).val(tr.conf.getAttribute('id'));
        if ( tr.conf.getAttribute('var') == "true") $("[name=var]", dialog).attr('checked','1').trigger('change'); 
        else $("[name=var]", dialog).removeAttr('checked').trigger('change');
        $("[name=value]", dialog).val(tr.conf.getAttribute('value'));
        break;
      case 'send-email' :
        $("[name=to]", dialog).val(tr.conf.getAttribute('to'));
        $("[name=subject]", dialog).val(tr.conf.getAttribute('subject'));
        if ( tr.conf.getAttribute('var') == "true") $("[name=var]", dialog).attr('checked','1').trigger('change'); 
        else $("[name=var]", dialog).removeAttr('checked').trigger('change');
        $("[name=message]", dialog).val(tr.conf.textContent);
        break;
      case 'dim-up' :
        $("[name=id]", dialog).val(tr.conf.getAttribute('id'));
        $("[name=start]", dialog).val(tr.conf.getAttribute('start'));
        $("[name=stop]", dialog).val(tr.conf.getAttribute('stop'));
        $("[name=duration]", dialog).val(tr.conf.getAttribute('duration'));
        break;
      case 'shell-cmd' :
        if ( tr.conf.getAttribute('var') == "true") $("[name=var]", dialog).attr('checked','1').trigger('change'); 
        else $("[name=var]", dialog).removeAttr('checked').trigger('change');
        $("[name=cmd]", dialog).val(tr.conf.getAttribute('cmd'));
        break;
      case 'ioport-tx' :
        $("[name=ioport]", dialog).val(tr.conf.getAttribute('ioport'));
        if ( tr.conf.getAttribute('hex') == "true") $("[name=hex]", dialog).attr('checked','1').trigger('change'); 
        else $("[name=hex]", dialog).removeAttr('checked').trigger('change');
        if ( tr.conf.getAttribute('var') == "true") $("[name=var]", dialog).attr('checked','1').trigger('change'); 
        else $("[name=var]", dialog).removeAttr('checked').trigger('change');
        $("[name=data]", dialog).val(tr.conf.getAttribute('data'));
        break;
      case 'script' :
        $("[name=script]", dialog).val(tr.conf.textContent);
        break;
      case 'cancel' :
//        $('#tab-rules-cancel-action-value').val(div.cancel_rule);
        break;
      case 'formula' :
        $("[name=id]", dialog).val(tr.conf.getAttribute('id'));
        $("[name=x]", dialog).val(tr.conf.getAttribute('x'));
        $("[name=y]", dialog).val(tr.conf.getAttribute('y'));
        $("[name=a]", dialog).val(tr.conf.getAttribute('a'));
        $("[name=b]", dialog).val(tr.conf.getAttribute('b'));
        $("[name=c]", dialog).val(tr.conf.getAttribute('c'));
        $("[name=m]", dialog).val(tr.conf.getAttribute('m'));
        $("[name=n]", dialog).val(tr.conf.getAttribute('n'));
        break;
      case 'start-actionlist' :
        $("[name=rule-id]", dialog).val(tr.conf.getAttribute('rule-id'));
				if (tr.conf.getAttribute('list')=="true") $("[name=list]", dialog).attr('checked', '1'); else $("[name=list]", dialog).removeAttr('checked');
        break;
      case 'set-rule-active' :
        $("[name=rule-id]", dialog).val(tr.conf.getAttribute('rule-id'));
				if (tr.conf.getAttribute('active')=="yes") $("[name=active]", dialog).attr('checked', '1'); else $("[name=active]", dialog).removeAttr('checked');
        break;
    };
    
    dialog.dialog('open');
  },
  
  processCloseDialog: function(dialog) {
    var conf=dialog[0].editing.conf;
    
    switch (conf.getAttribute('type')) {
	    case 'set-value':
	    	conf.setAttribute('id', $("[name=id]", dialog).val());
	    	conf.setAttribute('value', $("[name=value]", dialog).val());
        if ($("[name=values]", dialog).css('display')!='none')
          conf.setAttribute('value', $("[name=values]", dialog).val());
        else
          conf.setAttribute('value', $("[name=value]", dialog).val());
	    	break;
	    case 'copy-value':
	    	conf.setAttribute('from', $("[name=from]", dialog).val());
	    	conf.setAttribute('to', $("[name=to]", dialog).val());
	    	break;
	    case 'toggle-value':
	    	conf.setAttribute('id', $("[name=id]", dialog).val());
	    	break;
	    case 'set-string':
	    	conf.setAttribute('id', $("[name=id]", dialog).val());
	    	conf.setAttribute('value', $("[name=value]", dialog).val());
	    	break;
	    case 'send-read-request':
	    	conf.setAttribute('id', $("[name=id]", dialog).val());
	    	break;
	    case 'cycle-on-off':
	    	conf.setAttribute('id', $("[name=id]", dialog).val());
	    	conf.setAttribute('on', $("[name=on]", dialog).val());
	    	conf.setAttribute('off', $("[name=off]", dialog).val());
	    	conf.setAttribute('count', $("[name=count]", dialog).val());
	    	break;
	    case 'send-sms':
	    	conf.setAttribute('id', $("[name=id]", dialog).val());
	    	conf.setAttribute('var', $("[name=var]", dialog).is(':checked'));
	    	conf.setAttribute('value', $("[name=value]", dialog).val());
	    	break;
	    case 'send-email':
	    	conf.setAttribute('to', $("[name=to]", dialog).val());
	    	conf.setAttribute('subject', $("[name=subject]", dialog).val());
	    	conf.setAttribute('var', $("[name=var]", dialog).is(':checked'));
	    	conf.textContent= $("[name=message]", dialog).val();
	    	break;
	    case 'dim-up':
	    	conf.setAttribute('id', $("[name=id]", dialog).val());
	    	conf.setAttribute('start', $("[name=start]", dialog).val());
	    	conf.setAttribute('stop', $("[name=stop]", dialog).val());
	    	conf.setAttribute('duration', $("[name=duration]", dialog).val());
	    	break;
	    case 'shell-cmd':
	    	conf.setAttribute('var', $("[name=var]", dialog).is(':checked'));
	    	conf.setAttribute('cmd', $("[name=cmd]", dialog).val() );
	    	break;
	    case 'ioport-tx':
	    	conf.setAttribute('ioport', $("[name=ioport]", dialog).val());
	    	conf.setAttribute('hex', $("[name=hex]", dialog).is(':checked'));
	    	conf.setAttribute('var', $("[name=var]", dialog).is(':checked'));
	    	conf.setAttribute('data', $("[name=data]", dialog).val());
	    	break;
	    case 'script':
	    	conf.textContent='<![CDATA[' + $("[name=script]", dialog).val() + ']]>';
	    	break;
	    case 'formula':
	    	conf.setAttribute('id', $("[name=id]", dialog).val());
	    	if ($("[name=x]", dialog).val()!='') conf.setAttribute('x', $("[name=x]", dialog).val()); else conf.removeAttribute('x');
	    	if ($("[name=y]", dialog).val()!='') conf.setAttribute('y', $("[name=y]", dialog).val()); else conf.removeAttribute('y');
	    	if ($("[name=a]", dialog).val()!='') conf.setAttribute('a', $("[name=a]", dialog).val()); else conf.removeAttribute('a');
	    	if ($("[name=b]", dialog).val()!='') conf.setAttribute('b', $("[name=b]", dialog).val()); else conf.removeAttribute('b');
	    	if ($("[name=c]", dialog).val()!='') conf.setAttribute('c', $("[name=c]", dialog).val()); else conf.removeAttribute('c');
	    	if ($("[name=m]", dialog).val()!='') conf.setAttribute('m', $("[name=m]", dialog).val()); else conf.removeAttribute('m');
	    	if ($("[name=n]", dialog).val()!='') conf.setAttribute('n', $("[name=n]", dialog).val()); else conf.removeAttribute('n');
	    	break;
	    case 'start-actionlist':
	    	conf.setAttribute('rule-id', $("[name=rule-id]", dialog).val());
	    	conf.setAttribute('list', (($("[name=list]", dialog).is(':checked'))?'true':'false') );
	    	break;
	    case 'set-rule-active':
	    	conf.setAttribute('rule-id', $("[name=rule-id]", dialog).val());
	    	conf.setAttribute('active', (($("[name=active]", dialog).is(':checked'))?'yes':'no') );
	    	break;
		}
		
		$(".description", dialog[0].editing).html(actionEditor.getActionDescription(conf));
  },
  
	open: function(conf, subPageObjects) {
		actionEditor.subPageObjects = typeof subPageObjects === "undefined" ? [ ] : subPageObjects;
		
		actionEditor.config=conf;
		
		$("#"+this.prefix+"action-dialog-list tbody").empty();
	
		$('action', actionEditor.config).each(function() {
			actionEditor.add(this);
		});
		
		if (this.prefix === '')
		  $("#action-dialog").dialog('open');
	},
	
	fillObjectsSelect: function(div) {
		$('.object-select', div).each(function() {
			var select=$(this);
			select.empty();
			var option=($('<option value="">None</option>'));
			select.append(option);
			var optgroup=$("<optgroup label='Objects'>");
			$('object', _objects).each(function() {
				var option=($('<option value="' + this.getAttribute('id') + '">' + ((this.textContent!="")?this.textContent:this.getAttribute('id')) + ' (' + this.getAttribute('type') + ')</option>'));
				option[0].type = this.getAttribute('type');
				optgroup.append(option);
			});
			select.append(optgroup);

			if (actionEditor.subPageObjects.length>0) {
				var optgroup=$("<optgroup label='Sub-page objects'>");
				$.each(actionEditor.subPageObjects, function() {
					var option=($('<option value="_' + this.id + '">' + this.value + '</option>'));
					optgroup.append(option);
				});
				select.append(optgroup);
			}
		});
	},
	
	fillIOPortsSelect: function(div) {
		
		if ($('.ioport-select', div).length>0) {
			var body = '<read><config><services><ioports /></services></config></read>';
			var req = jQuery.ajax({ type: 'post', url: 'linknx.php?action=cmd', data: body, processData: false, dataType: 'xml',
				success: function(responseXML, status) {
					var xmlResponse = responseXML.documentElement;
					if (xmlResponse.getAttribute('status') != 'error') {
						
						$('ioport', responseXML).each(function() {
							
							var ioport=this.getAttribute('id');
	
							$('.ioport-select', div).each(function() {
								var select=$(this);
								var option=($('<option value="' + ioport + '">' + ioport + '</option>'));
								select.append(option);
							});
						});
					}
					else
						messageBox(tr("Error: ")+responseXML.textContent, 'Erreur', 'alert');
					loading.hide();
				}
			});
		}
	}
}

jQuery(function($) {
  var actionsSelect=$('#action-dialog-select').get(0);
  actionsSelect.options[actionsSelect.options.length] = new Option(tr("Add an action"), "")
  for(key in actionEditor.actionsList) actionsSelect.options[actionsSelect.options.length] = new Option(actionEditor.actionsList[key], key);
  
  $('#action-dialog-select').change(function(e){
  	if (this.value!="")
  	{
	    var type = this.value;
	    actionEditor.edit(actionEditor.new(type).get(0), true);
	    this.value = "";
		}
  });

	$("#action-dialog").dialog({
		buttons: {
			Ok: function() {
				$( this ).dialog( "close" );
			}
		},
		title: "Action editor",
		width: 500,
		height: 300,
		modal: true,
		autoOpen: false,
		open: function() {
		},
		close: function() {
		}
	});
	
	$.each(actionEditor.actionsList, function(key, value) {
		$("#action-dialog-" + key + "-dialog").dialog({
			buttons: {
				Ok: function() {
					actionEditor.processCloseDialog($(this));
					actionEditor.subPageObjects= [ ];
					$( this ).dialog( "close" );
				},
				Cancel: function() {
					if (actionEditor.isNew) actionEditor.del($(this.editing));
					$( this ).dialog( "close" );
				}
			},
			title: "Edit " + value + " action",
			width: 450,
			modal: true,
			autoOpen: false
		});
	});
});