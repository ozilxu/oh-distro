<map version="0.9.0">
<!-- To view this file, download free mind mapping software FreeMind from http://freemind.sourceforge.net -->
<node CREATED="1364052253127" ID="ID_724683500" MODIFIED="1364052265519" TEXT="twolegodomoetry_estimate">
<node CREATED="1364052284649" ID="ID_836112003" MODIFIED="1364052288554" POSITION="right" TEXT="odometry">
<node CREATED="1364052291037" ID="ID_1212650550" MODIFIED="1364052292800" TEXT="parameters">
<node CREATED="1364052365815" ID="ID_1214166058" MODIFIED="1364052370894" TEXT="initialization"/>
</node>
<node CREATED="1364052293175" ID="ID_6057275" MODIFIED="1364052297055" TEXT="LCM interface">
<node CREATED="1364054407741" ID="ID_1117827571" MODIFIED="1364054413957" TEXT="LCM call back handler"/>
<node CREATED="1364063909398" ID="ID_277866391" MODIFIED="1364063963252" TEXT="state message publish">
<node CREATED="1364063964389" ID="ID_327093630" MODIFIED="1364063966646" TEXT="pelvis"/>
<node CREATED="1364063966916" ID="ID_1179025618" MODIFIED="1364063968316" TEXT="head"/>
</node>
</node>
<node CREATED="1364054291269" ID="ID_1144611093" MODIFIED="1364054301518" TEXT="variable members">
<node CREATED="1364054309320" ID="ID_522857762" MODIFIED="1364054318652" TEXT="current pelvis state"/>
<node CREATED="1364054319011" ID="ID_636856782" MODIFIED="1364054320815" TEXT="head state"/>
<node CREATED="1364054211510" ID="ID_373230797" MODIFIED="1364054224132" TEXT="list of footsteps"/>
<node CREATED="1364063493560" ID="ID_1094556787" MODIFIED="1364063506596" TEXT="ActiveLegTransform"/>
<node CREATED="1364063507107" ID="ID_120159657" MODIFIED="1364063518241" TEXT="ActiveFootPrintTransform"/>
<node CREATED="1364063689382" ID="ID_108025306" MODIFIED="1364063790602" TEXT="KinematicStatus">
<richcontent TYPE="NOTE"><html>
  <head>
    
  </head>
  <body>
    <p>
      Use this status to determine which elements of the odometry state to use in estimating the pelvis position
    </p>
  </body>
</html>
</richcontent>
<node CREATED="1364054305489" ID="ID_892577915" MODIFIED="1364054309001" TEXT="last footstep"/>
<node CREATED="1364054302920" ID="ID_1380005965" MODIFIED="1364054305105" TEXT="active foot"/>
</node>
</node>
<node CREATED="1364054371664" ID="ID_1896772293" MODIFIED="1364054374994" TEXT="member functions">
<node CREATED="1364052338743" ID="ID_1404700923" MODIFIED="1364054396757" TEXT="commands">
<node CREATED="1364052428843" ID="ID_285439907" MODIFIED="1364052430872" TEXT="start"/>
<node CREATED="1364052431431" ID="ID_1273266964" MODIFIED="1364052432390" TEXT="stop"/>
<node CREATED="1364052433182" ID="ID_1713168774" MODIFIED="1364052433955" TEXT="reset"/>
<node CREATED="1364052440891" ID="ID_1055602711" MODIFIED="1364052449640" TEXT="set initial conditions"/>
</node>
<node CREATED="1364054376733" ID="ID_461605376" MODIFIED="1364062634234" TEXT="getPelvisState()"/>
<node CREATED="1364054381245" ID="ID_886485203" MODIFIED="1364062631616" TEXT="getHeadState()"/>
<node CREATED="1364054776085" ID="ID_1372339576" MODIFIED="1364054817943" TEXT="updateStates()">
<richcontent TYPE="NOTE"><html>
  <head>
    
  </head>
  <body>
    <p>
      this function must update the internal states, based on information which is passed in as parameters
    </p>
  </body>
</html>
</richcontent>
<node CREATED="1364063521518" ID="ID_620065103" MODIFIED="1364063538235" TEXT="footprint + leg transforms"/>
</node>
<node CREATED="1364063540603" ID="ID_388195928" MODIFIED="1364063555555" TEXT="addNewFootprint"/>
<node CREATED="1364063556107" ID="ID_664010159" MODIFIED="1364063680369" TEXT="setActiveFoot()"/>
<node CREATED="1364052302214" ID="ID_241690413" MODIFIED="1364052305285" TEXT="computations model">
<node CREATED="1364052298118" ID="ID_1076183861" MODIFIED="1364052328907" TEXT="subsections">
<node CREATED="1364052394344" ID="ID_320765297" MODIFIED="1364052396608" TEXT="pelvis"/>
<node CREATED="1364052396934" ID="ID_1344346835" MODIFIED="1364052404955" TEXT="left leg">
<node CREATED="1364052410554" ID="ID_1782862603" MODIFIED="1364052411914" TEXT="foot"/>
</node>
<node CREATED="1364052405474" ID="ID_1845176034" MODIFIED="1364052408426" TEXT="right leg">
<node CREATED="1364052417366" ID="ID_117711835" MODIFIED="1364052419006" TEXT="foot"/>
</node>
</node>
</node>
<node CREATED="1364067579430" ID="ID_1001988418" MODIFIED="1364067585640" TEXT="addVectors()"/>
<node CREATED="1364067604626" ID="ID_75502862" MODIFIED="1364067611870" TEXT="publishResults()"/>
</node>
</node>
<node CREATED="1364054451472" ID="ID_1613661588" MODIFIED="1364054458205" POSITION="right" TEXT="noise and uncertainty"/>
<node CREATED="1364052316237" ID="ID_1786376600" MODIFIED="1364052318149" POSITION="right" TEXT="Use case"/>
<node CREATED="1364054160434" ID="ID_336407214" MODIFIED="1364054163329" POSITION="left" TEXT="types">
<node CREATED="1364054327069" ID="ID_795171560" MODIFIED="1364054335395" TEXT="state"/>
<node CREATED="1364054166802" ID="ID_1005264666" MODIFIED="1364054168096" TEXT="foot"/>
<node CREATED="1364054172061" ID="ID_1848778128" MODIFIED="1364054173961" TEXT="pelvis"/>
<node CREATED="1364054174256" ID="ID_1181560441" MODIFIED="1364054176250" TEXT="leg"/>
<node CREATED="1364054225925" ID="ID_558067319" MODIFIED="1364054229790" TEXT="footstep"/>
</node>
<node CREATED="1364054261953" ID="ID_1571488529" MODIFIED="1364054265660" POSITION="left" TEXT="requires">
<node CREATED="1364054267022" ID="ID_1131917519" MODIFIED="1364054271008" TEXT="Eigen"/>
<node CREATED="1364054496494" ID="ID_1944279972" MODIFIED="1364054500289" TEXT="lcmtypes"/>
<node CREATED="1364055091462" ID="ID_12589149" MODIFIED="1364055094562" TEXT="QuaternionLib"/>
</node>
</node>
</map>
