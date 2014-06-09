connect admin/12345@test/smp#vmy;
show tables;
select distinct SERVICE.Name,SOURCEALIAS.SourceAddress,SERVICE.TariffPlan,
SERVICEPROVIDER.Name,SERVICE.CHARGE_RULE_ID  
from SOURCEALIAS left outer join SERVICE on 
SOURCEALIAS.SERVICE_TSPID=SERVICE.TSPID  left outer 
join SERVICEPROVIDER on 
SERVICE.SERVICEPROVIDER_TSPID=SERVICEPROVIDER.TSPID;

 



connect admin/12345@test/root#vmy;
show db;
select * from cls;