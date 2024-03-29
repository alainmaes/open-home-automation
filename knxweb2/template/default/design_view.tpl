<html xmlns:v="urn:schemas-microsoft-com:vml">
<head>
		<meta http-equiv="Content-Type" content="text/html; charset=utf-8"/>
		
		<meta http-equiv="pragma" content="no-cache" />
		<meta http-equiv="cache-control" content="no-cache, must-revalidate" />
		
		<title>{$_config.title}</title>
		{foreach from=$cssList item=css}
		<link rel="stylesheet" type="text/css" href="{$css}" />
		{/foreach}
		<script type="text/javascript" >
		var tab_config = {$_config|json_encode};
		var _widgets = {$widgets|json_encode};
		</script>
		{foreach from=$jsList item=js}
		<script type="text/javascript" src="{$js}"></script>
		{/foreach}

<!-- OPEN_HOME_AUTOMATION -->
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=1" />
<meta name="apple-mobile-web-app-capable" content="yes" />
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent" />
<!-- OPEN_HOME_AUTOMATION -->

</head>
<!-- OPEN_HOME_AUTOMATION body -->
<body onload="window.scrollTo(0, 1);">

{if $_config.useJavaIfAvailable=='true'}
<applet code="org.knxweb.objectupdater.objectUpdater.class" archive="objectUpdater.jar" width="1" height="1">
    <param name="linknxHost" value="{$_config.linknx_host}">
    <param name="linknxPort" value="{$_config.linknx_port}">
</applet>
{/if}

<div id="widgetsTemplate" style="display: none;">

<div id='chart_div'></div>
	{foreach from=$widgets key=id item=i}
		{include file="widgets/$id/widget.html"}
	{/foreach}

</div>

<div id="test1">
</div>

<div class="menuTitle"></div>
<div id="notificationZone" class="notification"></div>


<div id="screen">
<img class="prev" src="images/previous.png" alt="prev" width="42" height="53" />

	<div id="zoneContainer">
	</div>

<img class="next" src="images/next.png" alt="next" width="42" height="53" />
</div>

</body>
</html>
